"""CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection, no final XOR)."""

_POLY = 0x1021
_INIT = 0xFFFF


def crc16(data: bytes) -> int:
    crc = _INIT
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ _POLY) if crc & 0x8000 else (crc << 1)
            crc &= 0xFFFF
    return crc
