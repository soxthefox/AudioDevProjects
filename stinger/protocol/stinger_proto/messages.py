"""Packet layer: header + typed messages (PROTOCOL.md sections 3 and 4)."""

import struct
from dataclasses import dataclass
from enum import IntEnum
from typing import ClassVar

from stinger_proto.errors import DecodeError, ErrorKind

HEADER_SIZE = 3
MAX_PACKET = 250
MAX_FRAME_LEDS = 81
MAX_BAR_ID = 16
BAR_UNASSIGNED = 0
BAR_ALL = 0xFF
MAC_SIZE = 6
BROADCAST_MAC = b"\xff" * MAC_SIZE

_HEADER = struct.Struct("<BH")


class MsgType(IntEnum):
    FRAME = 0x01
    SHOW = 0x02
    CONFIG = 0x03
    PING = 0x04
    CONFIG_MAC = 0x05
    HELLO = 0x81
    PONG = 0x82
    STATS = 0x83


class ConfigKey(IntEnum):
    BRIGHTNESS_CAP = 0x01
    FAILSAFE_TIMEOUT_MS = 0x02
    ASSIGN_ID = 0x03


def _is_bar_id(bar_id: int) -> bool:
    return 1 <= bar_id <= MAX_BAR_ID


def _require(ok: bool, detail: str) -> None:
    if not ok:
        raise DecodeError(ErrorKind.BAD_FIELD, detail)


def _config_key(raw: int) -> ConfigKey:
    try:
        return ConfigKey(raw)
    except ValueError:
        raise DecodeError(ErrorKind.BAD_FIELD, f"unknown config key {raw:#x}") from None


@dataclass(frozen=True)
class Frame:
    bar_id: int
    rgb: bytes  # R G B per pixel

    TYPE: ClassVar[MsgType] = MsgType.FRAME

    @property
    def led_count(self) -> int:
        return len(self.rgb) // 3

    def validate(self) -> None:
        _require(_is_bar_id(self.bar_id), f"FRAME bar_id {self.bar_id}")
        _require(len(self.rgb) % 3 == 0, "FRAME rgb length is not a multiple of 3")
        _require(self.led_count <= MAX_FRAME_LEDS, f"FRAME led_count {self.led_count}")

    def payload(self) -> bytes:
        return bytes([self.bar_id, self.led_count]) + self.rgb


@dataclass(frozen=True)
class Show:
    frame_seq: int

    TYPE: ClassVar[MsgType] = MsgType.SHOW
    _FORMAT: ClassVar[struct.Struct] = struct.Struct("<H")

    def validate(self) -> None:
        pass

    def payload(self) -> bytes:
        return self._FORMAT.pack(self.frame_seq)


@dataclass(frozen=True)
class Config:
    bar_id: int
    key: ConfigKey
    value: int

    TYPE: ClassVar[MsgType] = MsgType.CONFIG
    _FORMAT: ClassVar[struct.Struct] = struct.Struct("<BBH")

    def validate(self) -> None:
        _require(_is_bar_id(self.bar_id) or self.bar_id == BAR_ALL, f"CONFIG bar_id {self.bar_id}")

    def payload(self) -> bytes:
        return self._FORMAT.pack(self.bar_id, self.key, self.value)


@dataclass(frozen=True)
class ConfigMac:
    mac: bytes
    key: ConfigKey
    value: int

    TYPE: ClassVar[MsgType] = MsgType.CONFIG_MAC
    _FORMAT: ClassVar[struct.Struct] = struct.Struct("<6sBH")

    def validate(self) -> None:
        _require(len(self.mac) == MAC_SIZE, "CONFIG_MAC mac must be 6 bytes")

    def payload(self) -> bytes:
        return self._FORMAT.pack(self.mac, self.key, self.value)


@dataclass(frozen=True)
class Ping:
    t_ms: int

    TYPE: ClassVar[MsgType] = MsgType.PING
    _FORMAT: ClassVar[struct.Struct] = struct.Struct("<I")

    def validate(self) -> None:
        pass

    def payload(self) -> bytes:
        return self._FORMAT.pack(self.t_ms)


@dataclass(frozen=True)
class Pong:
    t_ms: int

    TYPE: ClassVar[MsgType] = MsgType.PONG
    _FORMAT: ClassVar[struct.Struct] = Ping._FORMAT

    def validate(self) -> None:
        pass

    def payload(self) -> bytes:
        return self._FORMAT.pack(self.t_ms)


@dataclass(frozen=True)
class Hello:
    bar_id: int
    mac: bytes
    led_count: int
    fw_version: int
    rssi: int

    TYPE: ClassVar[MsgType] = MsgType.HELLO
    _FORMAT: ClassVar[struct.Struct] = struct.Struct("<B6sBHb")

    def validate(self) -> None:
        _require(0 <= self.bar_id <= MAX_BAR_ID, f"HELLO bar_id {self.bar_id}")
        _require(len(self.mac) == MAC_SIZE, "HELLO mac must be 6 bytes")

    def payload(self) -> bytes:
        return self._FORMAT.pack(self.bar_id, self.mac, self.led_count, self.fw_version, self.rssi)


@dataclass(frozen=True)
class Stats:
    rx_packets: int
    crc_errors: int
    tx_failures: int

    TYPE: ClassVar[MsgType] = MsgType.STATS
    _FORMAT: ClassVar[struct.Struct] = struct.Struct("<III")

    def validate(self) -> None:
        pass

    def payload(self) -> bytes:
        return self._FORMAT.pack(self.rx_packets, self.crc_errors, self.tx_failures)


Message = Frame | Show | Config | ConfigMac | Ping | Pong | Hello | Stats


@dataclass(frozen=True)
class Packet:
    seq: int
    message: Message


def encode_packet(seq: int, message: Message) -> bytes:
    """Encode one packet. Raises DecodeError(BAD_FIELD) for out-of-range fields."""
    message.validate()
    try:
        return _HEADER.pack(message.TYPE, seq) + message.payload()
    except struct.error as err:
        raise DecodeError(ErrorKind.BAD_FIELD, str(err)) from None


def _unpack(fmt: struct.Struct, payload: bytes) -> tuple:
    if len(payload) != fmt.size:
        raise DecodeError(ErrorKind.BAD_LENGTH, f"payload {len(payload)} bytes, want {fmt.size}")
    return fmt.unpack(payload)


def _decode_frame(payload: bytes) -> Frame:
    if len(payload) < 2 or len(payload) != 2 + 3 * payload[1]:
        raise DecodeError(ErrorKind.BAD_LENGTH, "FRAME length does not match led_count")
    if payload[1] > MAX_FRAME_LEDS:
        raise DecodeError(ErrorKind.BAD_FIELD, f"FRAME led_count {payload[1]}")
    return Frame(bar_id=payload[0], rgb=bytes(payload[2:]))


def _decode_message(msg_type: MsgType, payload: bytes) -> Message:
    match msg_type:
        case MsgType.FRAME:
            return _decode_frame(payload)
        case MsgType.SHOW:
            return Show(*_unpack(Show._FORMAT, payload))
        case MsgType.CONFIG:
            bar_id, key, value = _unpack(Config._FORMAT, payload)
            return Config(bar_id, _config_key(key), value)
        case MsgType.PING:
            return Ping(*_unpack(Ping._FORMAT, payload))
        case MsgType.PONG:
            return Pong(*_unpack(Pong._FORMAT, payload))
        case MsgType.CONFIG_MAC:
            mac, key, value = _unpack(ConfigMac._FORMAT, payload)
            return ConfigMac(mac, _config_key(key), value)
        case MsgType.HELLO:
            return Hello(*_unpack(Hello._FORMAT, payload))
        case MsgType.STATS:
            return Stats(*_unpack(Stats._FORMAT, payload))


def decode_packet(data: bytes) -> Packet:
    """Decode one packet, checking errors in PROTOCOL.md order."""
    if len(data) < HEADER_SIZE:
        raise DecodeError(ErrorKind.TOO_SHORT, f"{len(data)} bytes")
    raw_type, seq = _HEADER.unpack_from(data)
    try:
        msg_type = MsgType(raw_type)
    except ValueError:
        raise DecodeError(ErrorKind.UNKNOWN_TYPE, f"{raw_type:#x}") from None
    message = _decode_message(msg_type, bytes(data[HEADER_SIZE:]))
    message.validate()
    return Packet(seq, message)
