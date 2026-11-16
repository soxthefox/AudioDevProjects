"""Art-Net 4 ArtDmx and ArtSync packets (UDP 6454). Other opcodes are ignored."""

import struct

from stinger_bridge.dmx import DMX_MAX, DmxData, Sync

PORT = 6454
_ID = b"Art-Net\x00"
_OP_DMX = 0x5000
_OP_SYNC = 0x5200
_PROTOCOL_VERSION = 14
_DMX_DATA_AT = 18


def encode_dmx(universe: int, data: bytes, sequence: int = 0) -> bytes:
    if len(data) % 2:
        data += b"\x00"  # ArtDmx length must be even
    sub_uni, net = universe & 0xFF, (universe >> 8) & 0x7F
    return (
        _ID
        + struct.pack("<H", _OP_DMX)
        + struct.pack(">HBBBBH", _PROTOCOL_VERSION, sequence & 0xFF, 0, sub_uni, net, len(data))
        + data
    )


def encode_sync() -> bytes:
    return _ID + struct.pack("<H", _OP_SYNC) + struct.pack(">HBB", _PROTOCOL_VERSION, 0, 0)


def decode(packet: bytes) -> DmxData | Sync | None:
    """An ArtDmx or ArtSync, or None for anything else (including malformed input)."""
    if len(packet) < 12 or not packet.startswith(_ID):
        return None
    (opcode,) = struct.unpack_from("<H", packet, 8)
    if opcode == _OP_SYNC:
        return Sync()
    if opcode != _OP_DMX or len(packet) < _DMX_DATA_AT:
        return None
    sub_uni, net = packet[14], packet[15]
    (length,) = struct.unpack_from(">H", packet, 16)
    length = min(length, DMX_MAX)
    data = packet[_DMX_DATA_AT : _DMX_DATA_AT + length]
    if len(data) < length:
        return None
    return DmxData(((net & 0x7F) << 8) | sub_uni, bytes(data))
