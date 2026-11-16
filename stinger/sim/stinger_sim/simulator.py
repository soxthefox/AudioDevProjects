"""Wires virtual bars, radio and gateway together, and serves the gateway's serial link over TCP."""

import asyncio
import base64
import random
import time

from stinger_config import Installation
from stinger_proto import encode_packet

from stinger_sim.bar import BarSettings, VirtualBar
from stinger_sim.gateway import GatewaySettings, VirtualGateway
from stinger_sim.metrics import SimMetrics
from stinger_sim.radio import SimRadio

FW_VERSION = 0x0100
TICK_S = 0.005
# Simulated signal strength: nearer bars (lower IDs) are a little stronger.
BASE_RSSI = -40
RSSI_STEP_PER_BAR = 2
SERIAL_BITS_PER_BYTE = 10  # 8N1
MAX_LOSS = 0.3


def now_ms() -> int:
    return int(time.monotonic() * 1000)


def sim_mac(index: int) -> bytes:
    """Locally administered MACs 02:53:54:00:00:NN ("ST" for Stinger)."""
    return bytes([0x02, 0x53, 0x54, 0x00, 0x00, index])


class Simulator:
    def __init__(self, installation: Installation, bar_count: int, baud: int, seed: int) -> None:
        self.installation = installation
        self.metrics = SimMetrics()
        settings = BarSettings(
            led_count=installation.leds_per_bar,
            hard_limit=installation.brightness_hard_limit,
            failsafe_timeout_ms=installation.failsafe_timeout_ms,
            failsafe_fade_ms=installation.failsafe_fade_ms,
            hello_interval_ms=installation.hello_interval_ms,
            fw_version=FW_VERSION,
        )
        self.bars = [
            VirtualBar(sim_mac(i), i, settings, on_latch=self._on_latch)
            for i in range(1, bar_count + 1)
        ]
        self.radio = SimRadio(self.bars, now_ms, random.Random(seed))
        self.gateway = VirtualGateway(
            self.radio,
            self._write_serial,
            GatewaySettings(
                config_retries=installation.config_retries,
                stats_interval_ms=installation.stats_interval_ms,
                offline_after_ms=installation.offline_after_ms,
            ),
            observer=self.metrics.host_message,
        )
        self._bytes_per_s = baud / SERIAL_BITS_PER_BYTE if baud else 0.0
        self._client: asyncio.StreamWriter | None = None
        self._hello_seq = 0

    # ---- controls (from the browser view or tests) ----

    def set_plugged(self, bar_id: int, plugged: bool) -> None:
        bar = self._bar(bar_id)
        if plugged and bar.mac in self.radio.unplugged:
            self.radio.unplugged.discard(bar.mac)
            bar.reboot()  # power returns: the bar boots again
            self.metrics.reset_bar(bar.mac)
        elif not plugged:
            self.radio.unplugged.add(bar.mac)

    def set_loss(self, loss: float) -> None:
        self.radio.loss = min(max(loss, 0.0), MAX_LOSS)

    def set_brightness_cap(self, cap: int) -> None:
        for bar in self.bars:
            bar.runtime_cap = min(max(cap, 0), 255)

    def command(self, cmd: dict) -> None:
        match cmd["cmd"]:
            case "plug":
                self.set_plugged(int(cmd["bar"]), bool(cmd["plugged"]))
            case "loss":
                self.set_loss(float(cmd["value"]))
            case "cap":
                self.set_brightness_cap(int(cmd["value"]))

    # ---- running ----

    async def serve_serial(
        self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        """One TCP client plays the laptop end of the USB serial link. A new one replaces it."""
        if self._client is not None:
            self._client.close()
        self._client = writer
        link_free_s = time.monotonic()
        try:
            while data := await reader.read(4096):
                self.metrics.bytes_arrived(now_ms())
                for chunk in _split_after_delimiters(data):
                    link_free_s = await self._pace(chunk, link_free_s)
                    self.gateway.receive_serial(chunk, now_ms())
        except ConnectionError:
            pass
        finally:
            if self._client is writer:
                self._client = None
            writer.close()

    async def run_ticks(self) -> None:
        while True:
            self.tick(now_ms())
            await asyncio.sleep(TICK_S)

    def tick(self, now: int) -> None:
        for index, bar in enumerate(self.bars):
            if not self.radio.is_plugged(bar):
                continue
            hello = bar.poll_hello(now)
            if hello is None or self.radio.drops_packet():
                continue
            self._hello_seq = (self._hello_seq + 1) & 0xFFFF
            rssi = BASE_RSSI - RSSI_STEP_PER_BAR * index
            self.gateway.receive_radio(encode_packet(self._hello_seq, hello), rssi, now)
        self.gateway.tick(now)

    def snapshot(self, now: int) -> dict:
        latency = self.metrics.latency_ms(now)
        return {
            "connected": self._client is not None,
            "loss": self.radio.loss,
            "hard_limit": self.installation.brightness_hard_limit,
            "cap": self.bars[0].runtime_cap if self.bars else 0,
            "latency_ms": None if latency is None else round(latency, 1),
            "gateway": {
                "rx_packets": self.gateway.rx_packets,
                "crc_errors": self.gateway.crc_errors,
                "tx_failures": self.gateway.tx_failures,
            },
            "bars": [self._bar_state(bar, now) for bar in self.bars],
        }

    def _bar_state(self, bar: VirtualBar, now: int) -> dict:
        metrics = self.metrics.bars.get(bar.mac)
        return {
            "id": bar.bar_id,
            "mac": bar.mac.hex(":"),
            "plugged": self.radio.is_plugged(bar),
            "fps": metrics.fps(now) if metrics else 0,
            "dropped": metrics.dropped if metrics else 0,
            "pixels": base64.b64encode(bar.pixels(now)).decode(),
        }

    def _bar(self, bar_id: int) -> VirtualBar:
        for bar in self.bars:
            if bar.bar_id == bar_id:
                return bar
        raise KeyError(f"no bar with ID {bar_id}")

    def _on_latch(self, bar: VirtualBar, frame_seq: int, new_frame: bool, now: int) -> None:
        self.metrics.latched(bar.mac, frame_seq, new_frame, now)

    def _write_serial(self, data: bytes) -> None:
        if self._client is not None and not self._client.is_closing():
            self._client.write(data)

    async def _pace(self, chunk: bytes, link_free_s: float) -> float:
        """Hold each frame until a real serial line at the configured baud would deliver it."""
        if not self._bytes_per_s:
            return link_free_s
        link_free_s = max(link_free_s, time.monotonic()) + len(chunk) / self._bytes_per_s
        delay = link_free_s - time.monotonic()
        if delay > 0:
            await asyncio.sleep(delay)
        return link_free_s


def _split_after_delimiters(data: bytes) -> list[bytes]:
    """Split a byte chunk into pieces that each end at a 0x00 delimiter (last may not)."""
    pieces = data.split(b"\x00")
    out = [p + b"\x00" for p in pieces[:-1]]
    if pieces[-1]:
        out.append(pieces[-1])
    return out
