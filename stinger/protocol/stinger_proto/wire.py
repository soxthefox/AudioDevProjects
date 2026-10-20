"""Serial framing: COBS(packet || crc16_le) || 0x00 (PROTOCOL.md section 2)."""

import struct
from collections.abc import Iterator

from stinger_proto.cobs import cobs_decode, cobs_encode
from stinger_proto.crc import crc16
from stinger_proto.errors import DecodeError, ErrorKind
from stinger_proto.messages import HEADER_SIZE, MAX_PACKET

CRC_SIZE = 2
DELIMITER = 0
# COBS adds one byte per 254 input bytes, at least one.
MAX_WIRE_BODY = MAX_PACKET + MAX_PACKET // 254 + 1


def encode_wire(packet: bytes) -> bytes:
    if len(packet) > MAX_PACKET:
        raise ValueError(f"packet is {len(packet)} bytes, limit is {MAX_PACKET}")
    return cobs_encode(packet + struct.pack("<H", crc16(packet))) + bytes([DELIMITER])


def decode_wire_body(body: bytes) -> bytes:
    """Decode one frame (without its delimiter) to a packet, checking the CRC."""
    if len(body) > MAX_WIRE_BODY:
        raise DecodeError(ErrorKind.FRAMING, "frame longer than the maximum")
    decoded = cobs_decode(body)
    if len(decoded) < HEADER_SIZE + CRC_SIZE:
        raise DecodeError(ErrorKind.FRAMING, "frame shorter than header plus CRC")
    packet, (crc,) = decoded[:-CRC_SIZE], struct.unpack("<H", decoded[-CRC_SIZE:])
    if crc16(packet) != crc:
        raise DecodeError(ErrorKind.CRC)
    return packet


class WireDecoder:
    """Splits a serial byte stream into packets; counts and skips bad frames."""

    def __init__(self) -> None:
        self._buf = bytearray()
        self.errors = 0

    def feed(self, data: bytes) -> Iterator[bytes | DecodeError]:
        """Yield each complete packet, or the DecodeError for each dropped frame."""
        for byte in data:
            if byte != DELIMITER:
                # Keep one byte past the limit so an oversize frame is detected.
                if len(self._buf) <= MAX_WIRE_BODY:
                    self._buf.append(byte)
                continue
            body = bytes(self._buf)
            self._buf.clear()
            if not body:
                continue
            try:
                yield decode_wire_body(body)
            except DecodeError as err:
                self.errors += 1
                yield err
