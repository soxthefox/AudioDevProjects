"""Art-Net and sACN parsing, checked against hand-laid-out bytes from the specs."""

import struct

from stinger_bridge import artnet, sacn
from stinger_bridge.dmx import DmxData, Sync

CID = bytes(range(16))


def test_artdmx_from_spec_layout() -> None:
    packet = (
        b"Art-Net\x00"
        + b"\x00\x50"  # OpDmx, little-endian
        + b"\x00\x0e"  # protocol version 14, big-endian
        + b"\x07\x00"  # sequence, physical
        + b"\x23\x01"  # SubUni 0x23, Net 1 -> port-address 0x123
        + b"\x00\x04"  # length 4, big-endian
        + b"\x0a\x0b\x0c\x0d"
    )
    assert artnet.decode(packet) == DmxData(0x123, b"\x0a\x0b\x0c\x0d")
    assert artnet.encode_dmx(0x123, b"\x0a\x0b\x0c\x0d", sequence=7) == packet


def test_artdmx_odd_length_is_padded_to_even() -> None:
    assert artnet.decode(artnet.encode_dmx(0, b"\x01\x02\x03")) == DmxData(0, b"\x01\x02\x03\x00")


def test_artsync_and_junk() -> None:
    assert artnet.decode(artnet.encode_sync()) == Sync()
    assert artnet.decode(b"Art-Net\x00\x00\x20\x00\x0e\x00\x00") is None  # ArtPoll
    assert artnet.decode(b"hello") is None
    truncated = artnet.encode_dmx(0, bytes(100))[:-10]
    assert artnet.decode(truncated) is None


def test_sacn_data_field_offsets() -> None:
    packet = sacn.encode_dmx(513, b"\x11\x22\x33", sequence=9, cid=CID)
    assert len(packet) == 126 + 3
    assert packet[4:16] == b"ASC-E1.17\x00\x00\x00"
    assert struct.unpack_from(">I", packet, 18)[0] == 0x00000004
    assert packet[22:38] == CID
    assert struct.unpack_from(">I", packet, 40)[0] == 0x00000002
    assert packet[111] == 9
    assert struct.unpack_from(">H", packet, 113)[0] == 513
    assert packet[117] == 0x02 and packet[118] == 0xA1
    assert struct.unpack_from(">H", packet, 123)[0] == 4  # start code + 3 channels
    assert packet[125] == 0
    assert sacn.decode(packet) == DmxData(513, b"\x11\x22\x33")


def test_sacn_ignores_preview_terminated_and_other_start_codes() -> None:
    packet = bytearray(sacn.encode_dmx(1, b"\xff" * 3, cid=CID))
    for options in (0x80, 0x40):
        packet[112] = options
        assert sacn.decode(bytes(packet)) is None
    packet[112] = 0
    packet[125] = 0xDD  # per-address priority, not dimmer data
    assert sacn.decode(bytes(packet)) is None


def test_sacn_sync_and_multicast_group() -> None:
    assert sacn.decode(sacn.encode_sync(7999, cid=CID)) == Sync()
    assert sacn.multicast_group(1) == "239.255.0.1"
    assert sacn.multicast_group(300) == "239.255.1.44"
