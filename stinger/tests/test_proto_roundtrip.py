"""Round-trip and corrupt-input properties of the Python codec."""

import random

import pytest
from stinger_proto import (
    BAR_ALL,
    MAX_BAR_ID,
    MAX_FRAME_LEDS,
    MAX_PACKET,
    Config,
    ConfigKey,
    ConfigMac,
    DecodeError,
    ErrorKind,
    Frame,
    Hello,
    Message,
    Ping,
    Pong,
    Show,
    Stats,
    WireDecoder,
    cobs_decode,
    cobs_encode,
    decode_packet,
    encode_packet,
    encode_wire,
)
from vectors import VECTORS

SEED = 1234


def random_message(rng: random.Random) -> Message:
    u16, u32 = (lambda: rng.randrange(1 << 16)), (lambda: rng.randrange(1 << 32))
    mac = rng.randbytes(6)
    key = rng.choice(list(ConfigKey))
    return rng.choice(
        [
            Frame(rng.randint(1, MAX_BAR_ID), rng.randbytes(3 * rng.randint(0, MAX_FRAME_LEDS))),
            Show(u16()),
            Config(rng.choice([*range(1, MAX_BAR_ID + 1), BAR_ALL]), key, u16()),
            ConfigMac(mac, key, u16()),
            Ping(u32()),
            Pong(u32()),
            Hello(
                rng.randint(0, MAX_BAR_ID), mac, rng.randrange(256), u16(), rng.randint(-128, 127)
            ),
            Stats(u32(), u32(), u32()),
        ]
    )


def test_random_messages_round_trip_through_wire() -> None:
    rng = random.Random(SEED)
    packets = [encode_packet(rng.randrange(1 << 16), random_message(rng)) for _ in range(500)]
    stream = b"".join(encode_wire(p) for p in packets)
    assert all(len(p) <= MAX_PACKET for p in packets)
    assert list(WireDecoder().feed(stream)) == packets
    for p in packets:
        decoded = decode_packet(p)
        assert encode_packet(decoded.seq, decoded.message) == p


def test_random_bytes_round_trip_through_cobs() -> None:
    rng = random.Random(SEED)
    for _ in range(500):
        data = bytes(rng.choice([0, 0, rng.randrange(256)]) for _ in range(rng.randrange(600)))
        encoded = cobs_encode(data)
        assert 0 not in encoded
        assert cobs_decode(encoded) == data


@pytest.mark.parametrize("case", VECTORS["packets"], ids=[c["name"] for c in VECTORS["packets"]])
def test_every_single_bit_flip_is_rejected(case: dict) -> None:
    wire = bytes.fromhex(case["wire"])
    for pos in range(len(wire) - 1):  # leave the delimiter alone
        for bit in range(8):
            corrupt = bytearray(wire)
            corrupt[pos] ^= 1 << bit
            results = list(WireDecoder().feed(bytes(corrupt)))
            assert results, "a corrupted frame must not vanish silently"
            assert all(isinstance(r, DecodeError) for r in results), (pos, bit)


def test_decoder_resyncs_after_garbage() -> None:
    good = encode_wire(encode_packet(5, Show(42)))
    garbage = bytes(range(1, 256)) * 2 + b"\x00" + b"\x07\x01\x00"
    results = list(WireDecoder().feed(garbage + good + good))
    assert [r for r in results if isinstance(r, bytes)] == [encode_packet(5, Show(42))] * 2


def test_byte_at_a_time_matches_whole_stream() -> None:
    stream = b"".join(bytes.fromhex(c["wire"]) for c in VECTORS["packets"])
    whole = list(WireDecoder().feed(stream))
    decoder = WireDecoder()
    single = [r for b in stream for r in decoder.feed(bytes([b]))]
    assert single == whole


def test_empty_frames_are_ignored() -> None:
    decoder = WireDecoder()
    assert list(decoder.feed(b"\x00\x00\x00")) == []
    assert decoder.errors == 0


@pytest.mark.parametrize(
    "message",
    [
        Frame(0, b""),
        Frame(17, b""),
        Frame(1, b"\x01" * (3 * (MAX_FRAME_LEDS + 1))),
        Frame(1, b"\x01\x02"),
        Config(0, ConfigKey.ASSIGN_ID, 1),
        Hello(17, b"\x00" * 6, 60, 0, 0),
        ConfigMac(b"\x00" * 5, ConfigKey.ASSIGN_ID, 1),
        Show(1 << 16),
    ],
)
def test_encoder_refuses_invalid_fields(message: Message) -> None:
    with pytest.raises(DecodeError) as err:
        encode_packet(0, message)
    assert err.value.kind == ErrorKind.BAD_FIELD
