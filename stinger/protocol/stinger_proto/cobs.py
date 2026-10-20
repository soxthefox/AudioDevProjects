"""Consistent Overhead Byte Stuffing (Cheshire and Baker), no zero-pair elimination."""

from stinger_proto.errors import DecodeError, ErrorKind

_MAX_RUN = 254


def cobs_encode(data: bytes) -> bytes:
    out = bytearray()
    block = bytearray()
    for i, byte in enumerate(data):
        if byte:
            block.append(byte)
        if not byte or len(block) == _MAX_RUN:
            out.append(len(block) + 1)
            out += block
            block.clear()
            # A full block at the very end is not followed by another code byte.
            if byte and i + 1 == len(data):
                return bytes(out)
    out.append(len(block) + 1)
    out += block
    return bytes(out)


def cobs_decode(data: bytes) -> bytes:
    out = bytearray()
    i = 0
    while i < len(data):
        code = data[i]
        i += 1
        if code == 0:
            raise DecodeError(ErrorKind.FRAMING, "zero code byte")
        run = data[i : i + code - 1]
        if len(run) != code - 1 or 0 in run:
            raise DecodeError(ErrorKind.FRAMING, "block runs past end or contains zero")
        out += run
        i += code - 1
        if code != 0xFF and i < len(data):
            out.append(0)
    return bytes(out)
