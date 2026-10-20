"""The Python codec against the golden vectors (PROTOCOL.md section 9)."""

import binascii
import importlib.util

import pytest
from stinger_proto import (
    DecodeError,
    ErrorKind,
    WireDecoder,
    cobs_decode,
    cobs_encode,
    crc16,
    decode_packet,
    encode_packet,
    encode_wire,
)
from vectors import VECTORS, VECTORS_DIR, packet_from_json


def ids(cases: list[dict]) -> list[str]:
    return [c.get("name", str(i)) for i, c in enumerate(cases)]


def test_crc_check_value() -> None:
    check = VECTORS["crc_check"]
    data = check["ascii"].encode()
    assert crc16(data) == check["crc"]
    assert crc16(data) == binascii.crc_hqx(data, 0xFFFF)


@pytest.mark.parametrize("case", VECTORS["cobs"])
def test_cobs(case: dict) -> None:
    decoded, encoded = bytes.fromhex(case["decoded"]), bytes.fromhex(case["encoded"])
    assert cobs_encode(decoded) == encoded
    assert cobs_decode(encoded) == decoded


@pytest.mark.parametrize("case", VECTORS["packets"], ids=ids(VECTORS["packets"]))
def test_packet_decodes_to_meaning(case: dict) -> None:
    assert decode_packet(bytes.fromhex(case["packet"])) == packet_from_json(case["message"])


@pytest.mark.parametrize("case", VECTORS["packets"], ids=ids(VECTORS["packets"]))
def test_meaning_encodes_to_packet(case: dict) -> None:
    expected = packet_from_json(case["message"])
    assert encode_packet(expected.seq, expected.message).hex() == case["packet"]


@pytest.mark.parametrize("case", VECTORS["packets"], ids=ids(VECTORS["packets"]))
def test_packet_wire_framing(case: dict) -> None:
    packet, wire = bytes.fromhex(case["packet"]), bytes.fromhex(case["wire"])
    assert encode_wire(packet) == wire
    assert list(WireDecoder().feed(wire)) == [packet]


@pytest.mark.parametrize("case", VECTORS["invalid_wire"], ids=ids(VECTORS["invalid_wire"]))
def test_invalid_wire_is_dropped_and_counted(case: dict) -> None:
    decoder = WireDecoder()
    (result,) = decoder.feed(bytes.fromhex(case["wire"]))
    assert isinstance(result, DecodeError)
    assert result.kind == ErrorKind[case["error"]]
    assert decoder.errors == 1


@pytest.mark.parametrize("case", VECTORS["invalid_packets"], ids=ids(VECTORS["invalid_packets"]))
def test_invalid_packet_is_rejected(case: dict) -> None:
    with pytest.raises(DecodeError) as err:
        decode_packet(bytes.fromhex(case["packet"]))
    assert err.value.kind == ErrorKind[case["error"]]


def test_c_header_is_up_to_date() -> None:
    spec = importlib.util.spec_from_file_location("gen_c_vectors", VECTORS_DIR / "gen_c_vectors.py")
    assert spec and spec.loader
    gen = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(gen)
    expected = gen.render(VECTORS)
    assert gen.HEADER_PATH.read_text() == expected, "run protocol/vectors/gen_c_vectors.py"
