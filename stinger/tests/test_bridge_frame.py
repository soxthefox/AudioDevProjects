"""Frame assembly from DMX, and send pacing."""

import itertools

import pytest
from stinger_bridge.dmx import DmxData
from stinger_bridge.frame import FrameAssembler, Pacer
from stinger_bridge.mapping import ARTNET, SACN, BarSource, Mapping, load_mapping

LEDS = 60


def test_default_mapping_is_one_universe_per_bar() -> None:
    mapping = load_mapping(bars=10, leds_per_bar=LEDS)
    assert [s.bar_id for s in mapping.bars_for(ARTNET, 0)] == [1]
    assert [s.bar_id for s in mapping.bars_for(ARTNET, 9)] == [10]
    assert [s.bar_id for s in mapping.bars_for(SACN, 1)] == [1]
    assert mapping.bars_for(ARTNET, 10) == []
    assert mapping.universes(SACN) == set(range(1, 11))


def test_mapping_rejects_channels_past_the_universe() -> None:
    with pytest.raises(ValueError):
        Mapping([BarSource(1, 0, 1, start_channel=400)], LEDS)


def test_assembler_copies_channels_and_pads_short_universes() -> None:
    mapping = Mapping([BarSource(1, 0, 1, 1), BarSource(2, 0, 2, 181)], LEDS)
    asm = FrameAssembler(mapping)
    data = bytes(range(256)) * 2
    assert asm.update(ARTNET, DmxData(0, data[:200]))
    assert asm.complete
    frame = asm.take()
    assert frame[1] == data[:180]
    assert frame[2] == data[180:200] + bytes(160)
    assert not asm.complete


def test_assembler_ignores_unmapped_universes() -> None:
    asm = FrameAssembler(load_mapping(bars=2, leds_per_bar=LEDS))
    assert not asm.update(ARTNET, DmxData(7, b"\xff" * 180))


def test_pacer_sends_complete_frames_at_once() -> None:
    p = Pacer(interval_ms=25, coalesce_ms=5, hold_ms=1000)
    p.input(100, ready=True)
    assert p.next_send() == 100


def test_pacer_waits_for_coalesce_window_on_partial_frames() -> None:
    p = Pacer(interval_ms=25, coalesce_ms=5, hold_ms=1000)
    p.input(100, ready=False)
    p.input(102, ready=False)
    assert p.next_send() == 105


def test_pacer_lets_a_slightly_early_frame_through() -> None:
    p = Pacer(interval_ms=25, coalesce_ms=5, hold_ms=1000)
    p.input(100, ready=True)
    p.sent(100)
    p.input(120, ready=True)  # input jitter: 20 ms instead of 25
    assert p.next_send() == 120


def test_pacer_never_exceeds_target_fps_on_average() -> None:
    p = Pacer(interval_ms=25, coalesce_ms=5, hold_ms=1000)
    sends = []
    for t in range(0, 10_000, 10):  # input at 100 fps for 10 s
        p.input(t, ready=True)
        if p.next_send() <= t:
            p.sent(t)
            sends.append(t)
    assert len(sends) <= 10_000 / 25 + Pacer.BURST
    assert min(b - a for a, b in itertools.pairwise(sends)) >= 10


def test_pacer_holds_last_frame_for_hold_time_then_stops() -> None:
    p = Pacer(interval_ms=25, coalesce_ms=5, hold_ms=1000)
    p.input(0, ready=True)
    p.sent(0)
    t = 0
    while (nxt := p.next_send()) is not None:
        t = nxt
        p.sent(t)
    assert 950 <= t <= 1000
