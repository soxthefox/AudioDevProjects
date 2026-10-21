"""sACN / ANSI E1.31 data and sync packets (UDP 5568). Other packets are ignored."""

import struct
import uuid

from stinger_bridge.dmx import DMX_MAX, DmxData, Sync

PORT = 5568
_ACN_ID = b"ASC-E1.17\x00\x00\x00"
_VECTOR_ROOT_DATA = 0x00000004
_VECTOR_ROOT_EXTENDED = 0x00000008
_VECTOR_FRAMING_DATA = 0x00000002
_VECTOR_FRAMING_SYNC = 0x00000001
_VECTOR_DMP = 0x02
_OPTION_PREVIEW = 0x80
_OPTION_TERMINATED = 0x40
_DATA_HEADER = 126  # bytes before channel 1
_SYNC_SIZE = 49
_SOURCE_NAME_SIZE = 64
_DEFAULT_PRIORITY = 100


def multicast_group(universe: int) -> str:
    return f"239.255.{(universe >> 8) & 0xFF}.{universe & 0xFF}"


def _root(vector: int, total: int, cid: bytes) -> bytes:
    return (
        struct.pack(">HH", 0x0010, 0)
        + _ACN_ID
        + struct.pack(">HI", 0x7000 | (total - 16), vector)
        + cid
    )


def encode_dmx(universe: int, data: bytes, sequence: int = 0, cid: bytes | None = None) -> bytes:
    cid = cid or uuid.uuid4().bytes
    total = _DATA_HEADER + len(data)
    framing = struct.pack(">HI", 0x7000 | (total - 38), _VECTOR_FRAMING_DATA) + (
        b"stinger".ljust(_SOURCE_NAME_SIZE, b"\x00")
    )
    framing += struct.pack(">BHBBH", _DEFAULT_PRIORITY, 0, sequence & 0xFF, 0, universe)
    dmp = struct.pack(">HBBHHHB", 0x7000 | (total - 115), _VECTOR_DMP, 0xA1, 0, 1, len(data) + 1, 0)
    return _root(_VECTOR_ROOT_DATA, total, cid) + framing + dmp + data


def encode_sync(sync_universe: int, sequence: int = 0, cid: bytes | None = None) -> bytes:
    cid = cid or uuid.uuid4().bytes
    framing = struct.pack(
        ">HIBHH",
        0x7000 | (_SYNC_SIZE - 38),
        _VECTOR_FRAMING_SYNC,
        sequence & 0xFF,
        sync_universe,
        0,
    )
    return _root(_VECTOR_ROOT_EXTENDED, _SYNC_SIZE, cid) + framing


def decode(packet: bytes) -> DmxData | Sync | None:
    """A data or sync packet, or None for anything else (including malformed input)."""
    if len(packet) < _SYNC_SIZE or packet[4:16] != _ACN_ID:
        return None
    (root_vector,) = struct.unpack_from(">I", packet, 18)
    (framing_vector,) = struct.unpack_from(">I", packet, 40)
    if root_vector == _VECTOR_ROOT_EXTENDED and framing_vector == _VECTOR_FRAMING_SYNC:
        return Sync()
    if root_vector != _VECTOR_ROOT_DATA or framing_vector != _VECTOR_FRAMING_DATA:
        return None
    if len(packet) < _DATA_HEADER or packet[117] != _VECTOR_DMP or packet[125] != 0:
        return None  # not DMP, or a non-zero (non-dimmer) start code
    if packet[112] & (_OPTION_PREVIEW | _OPTION_TERMINATED):
        return None
    (universe,) = struct.unpack_from(">H", packet, 113)
    (count,) = struct.unpack_from(">H", packet, 123)
    length = min(max(count - 1, 0), DMX_MAX)
    data = packet[_DATA_HEADER : _DATA_HEADER + length]
    if len(data) < length:
        return None
    return DmxData(universe, bytes(data))
