"""CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection, no final XOR).

binascii.crc_hqx is exactly this CRC when started at 0xFFFF, implemented in C. A pure-Python
bit loop cost about 2 ms per 10-bar frame, a fifth of the bridge's latency budget.
"""

import binascii

_INIT = 0xFFFF


def crc16(data: bytes) -> int:
    return binascii.crc_hqx(data, _INIT)
