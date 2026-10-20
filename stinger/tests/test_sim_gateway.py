"""The simulator's VirtualGateway forwarding rules (PROTOCOL.md section 5)."""

from stinger_proto import (
    BAR_ALL,
    BROADCAST_MAC,
    Config,
    ConfigKey,
    ConfigMac,
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
from stinger_sim.gateway import GatewaySettings, VirtualGateway

MAC_A = bytes.fromhex("0253540000a1")
MAC_B = bytes.fromhex("0253540000b2")
SETTINGS = GatewaySettings(config_retries=3, stats_interval_ms=1000, offline_after_ms=3000)


class FakeRadio:
    def __init__(self, acks: bool = True) -> None:
        self.acks = acks
        self.broadcasts: list[bytes] = []
        self.unicasts: list[tuple[bytes, bytes]] = []

    def broadcast(self, packet: bytes) -> bool:
        self.broadcasts.append(packet)
        return True

    def unicast(self, mac: bytes, packet: bytes) -> bool:
        self.unicasts.append((mac, packet))
        return self.acks


class Harness:
    def __init__(self, acks: bool = True) -> None:
        self.radio = FakeRadio(acks)
        self.out = bytearray()
        self.gateway = VirtualGateway(self.radio, self.out.extend, SETTINGS)

    def host_sends(self, message: Message, now_ms: int = 0) -> bytes:
        packet = encode_packet(1, message)
        self.gateway.receive_serial(encode_wire(packet), now_ms)
        return packet

    def bar_says_hello(self, bar_id: int, mac: bytes, now_ms: int = 0) -> None:
        self.gateway.receive_radio(encode_packet(9, Hello(bar_id, mac, 60, 0x0100, 0)), -55, now_ms)

    def to_host(self) -> list[Message]:
        packets = list(WireDecoder().feed(bytes(self.out)))
        self.out.clear()
        return [decode_packet(p).message for p in packets]


def test_frame_and_show_are_broadcast_unchanged() -> None:
    h = Harness()
    frame = h.host_sends(Frame(1, b"\x01\x02\x03"))
    show = h.host_sends(Show(7))
    assert h.radio.broadcasts == [frame, show]
    assert h.gateway.rx_packets == 2


def test_ping_is_answered_not_forwarded() -> None:
    h = Harness()
    h.host_sends(Ping(123456))
    assert h.to_host() == [Pong(123456)]
    assert h.radio.broadcasts == []


def test_hello_is_forwarded_with_rssi() -> None:
    h = Harness()
    h.bar_says_hello(4, MAC_A)
    assert h.to_host() == [Hello(4, MAC_A, 60, 0x0100, -55)]


def test_config_by_id_unicasts_to_the_mac_from_hello() -> None:
    h = Harness()
    h.bar_says_hello(4, MAC_A)
    packet = h.host_sends(Config(4, ConfigKey.BRIGHTNESS_CAP, 100))
    assert h.radio.unicasts == [(MAC_A, packet)]


def test_config_to_unknown_id_is_a_tx_failure() -> None:
    h = Harness()
    h.host_sends(Config(9, ConfigKey.BRIGHTNESS_CAP, 100))
    assert h.radio.unicasts == []
    assert h.gateway.tx_failures == 1


def test_config_all_reaches_only_recently_seen_bars() -> None:
    h = Harness()
    h.bar_says_hello(1, MAC_A, now_ms=0)
    h.bar_says_hello(2, MAC_B, now_ms=2500)
    h.host_sends(Config(BAR_ALL, ConfigKey.FAILSAFE_TIMEOUT_MS, 1000), now_ms=3500)
    assert [mac for mac, _ in h.radio.unicasts] == [MAC_B]


def test_unacknowledged_config_retries_three_times_then_counts_failure() -> None:
    h = Harness(acks=False)
    h.host_sends(ConfigMac(MAC_A, ConfigKey.ASSIGN_ID, 3))
    assert len(h.radio.unicasts) == 4
    assert h.gateway.tx_failures == 1


def test_config_mac_to_everyone_is_broadcast() -> None:
    h = Harness()
    packet = h.host_sends(ConfigMac(BROADCAST_MAC, ConfigKey.BRIGHTNESS_CAP, 50))
    assert h.radio.broadcasts == [packet]


def test_stats_every_interval_count_crc_errors() -> None:
    h = Harness()
    h.host_sends(Show(1))
    h.gateway.receive_serial(b"\x05\x11\x22\x00", 0)  # bad COBS
    h.gateway.tick(0)
    h.gateway.tick(999)
    h.gateway.tick(1000)
    assert h.to_host() == [Stats(1, 1, 0), Stats(1, 1, 0)]
