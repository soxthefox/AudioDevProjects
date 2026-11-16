"""A virtual gateway. Mirrors firmware/gateway/lib/gateway_logic (PROTOCOL.md section 5)."""

from collections.abc import Callable
from dataclasses import dataclass
from typing import Protocol

from stinger_proto import (
    BAR_ALL,
    BROADCAST_MAC,
    Config,
    ConfigMac,
    DecodeError,
    Frame,
    Hello,
    Message,
    Ping,
    Pong,
    Show,
    Stats,
    WireDecoder,
    decode_packet,
    encode_packet,
    encode_wire,
)


class Radio(Protocol):
    def broadcast(self, packet: bytes) -> bool:
        """Send to every bar without acknowledgement. False if the send failed locally."""
        ...

    def unicast(self, mac: bytes, packet: bytes) -> bool:
        """Send to one bar. True only if the bar acknowledged it."""
        ...


@dataclass(frozen=True)
class GatewaySettings:
    config_retries: int
    stats_interval_ms: int
    offline_after_ms: int


@dataclass
class _KnownBar:
    mac: bytes
    last_seen_ms: int


class VirtualGateway:
    def __init__(
        self,
        radio: Radio,
        write_serial: Callable[[bytes], None],
        settings: GatewaySettings,
        observer: Callable[[Message, int], None] | None = None,
    ) -> None:
        self._radio = radio
        self._write_serial = write_serial
        self._settings = settings
        self._observer = observer
        self._decoder = WireDecoder()
        self._bars: dict[int, _KnownBar] = {}
        self._seq = 0
        self._last_stats_ms: int | None = None
        self.rx_packets = 0
        self.tx_failures = 0

    @property
    def crc_errors(self) -> int:
        return self._decoder.errors

    def receive_serial(self, data: bytes, now_ms: int) -> None:
        for item in self._decoder.feed(data):
            if isinstance(item, DecodeError):
                continue
            self.rx_packets += 1
            try:
                message = decode_packet(item).message
            except DecodeError:
                continue
            if self._observer:
                self._observer(message, now_ms)
            self._handle_host_packet(item, message, now_ms)

    def receive_radio(self, packet: bytes, rssi: int, now_ms: int) -> None:
        try:
            message = decode_packet(packet).message
        except DecodeError:
            return
        if not isinstance(message, Hello):
            return
        if message.bar_id:
            self._bars[message.bar_id] = _KnownBar(message.mac, now_ms)
        self._send(Hello(message.bar_id, message.mac, message.led_count, message.fw_version, rssi))

    def tick(self, now_ms: int) -> None:
        if (
            self._last_stats_ms is not None
            and now_ms - self._last_stats_ms < self._settings.stats_interval_ms
        ):
            return
        self._last_stats_ms = now_ms
        self._send(Stats(self.rx_packets, self.crc_errors, self.tx_failures))

    def _handle_host_packet(self, packet: bytes, message: Message, now_ms: int) -> None:
        match message:
            case Frame() | Show():
                if not self._radio.broadcast(packet):
                    self.tx_failures += 1
            case Config():
                for mac in self._config_targets(message.bar_id, now_ms):
                    self._unicast_with_retries(mac, packet)
            case ConfigMac() if message.mac == BROADCAST_MAC:
                if not self._radio.broadcast(packet):
                    self.tx_failures += 1
            case ConfigMac():
                self._unicast_with_retries(message.mac, packet)
            case Ping():
                self._send(Pong(message.t_ms))

    def _config_targets(self, bar_id: int, now_ms: int) -> list[bytes]:
        if bar_id != BAR_ALL:
            known = self._bars.get(bar_id)
            if known is None:
                self.tx_failures += 1
                return []
            return [known.mac]
        recent = now_ms - self._settings.offline_after_ms
        return [b.mac for b in self._bars.values() if b.last_seen_ms >= recent]

    def _unicast_with_retries(self, mac: bytes, packet: bytes) -> None:
        for _ in range(1 + self._settings.config_retries):
            if self._radio.unicast(mac, packet):
                return
        self.tx_failures += 1

    def _send(self, message: Message) -> None:
        self._seq = (self._seq + 1) & 0xFFFF
        self._write_serial(encode_wire(encode_packet(self._seq, message)))
