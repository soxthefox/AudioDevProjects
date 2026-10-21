"""The test patterns."""

from stinger_tools.patterns import PATTERNS, chase, id_flash, strobe

RED = b"\xff\x00\x00"
LEDS, BARS = 60, 10


def test_every_pattern_fills_a_whole_bar() -> None:
    for pattern in PATTERNS.values():
        assert len(pattern(3, BARS, LEDS, 1.23, RED)) == 3 * LEDS


def test_id_flash_flashes_bar_n_n_times_per_cycle() -> None:
    step = 0.01
    cycle = BARS * 0.4 + 1.5
    for bar in (0, 2, 9):
        lit = [
            id_flash(bar, BARS, LEDS, i * step, RED)[:3] == RED for i in range(int(cycle / step))
        ]
        rising_edges = sum(1 for a, b in zip([False, *lit], lit, strict=False) if b and not a)
        assert rising_edges == bar + 1


def test_chase_crosses_from_one_bar_to_the_next() -> None:
    def lit_bars(t: float) -> list[int]:
        return [b for b in range(BARS) if RED in chase(b, BARS, LEDS, t, RED)]

    assert lit_bars(0.5) == [0]
    assert lit_bars(1.5) == [1]


def test_strobe_toggles() -> None:
    assert strobe(0, BARS, LEDS, 0.01, RED)[:3] == RED
    assert strobe(0, BARS, LEDS, 0.06, RED)[:3] == b"\0\0\0"
