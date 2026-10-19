"""Tests for the pure helpers in intra_frame.py (no video files needed)."""

from intra_frame import output_size, quarter_turns_of, source_frames_for


def test_constant_rate_source_maps_one_to_one():
    times = [n / 30 for n in range(30)]
    assert source_frames_for(times, fps=30, duration=1.0) == list(range(30))


def test_variable_rate_source_keeps_real_time():
    # 15 fps for the first half second, then 60 fps: at 30 fps output, early
    # frames repeat and later ones are skipped, but timing is preserved.
    times = [n / 15 for n in range(8)] + [0.5 + n / 60 for n in range(30)]
    picks = source_frames_for(times, fps=30, duration=1.0)
    assert len(picks) == 30
    assert picks[:4] == [0, 0, 1, 1]          # each 15 fps frame shown twice
    assert picks == sorted(picks)              # never goes back in time
    last_time = 29 / 30
    assert times[picks[-1]] <= last_time < times[picks[-1] + 1]   # frame on screen at that moment


def test_upright_iphone_rotation_is_one_clockwise_turn():
    assert quarter_turns_of(-90) == 3   # np.rot90 k=3 (= -1 mod 4)
    assert quarter_turns_of(0) == 0


def test_portrait_output_size_respects_licence_cap():
    assert output_size(1920, 1080, quarter_turns=3) == (720, 1280)
    assert output_size(1920, 1080, quarter_turns=0) == (1280, 720)
    assert output_size(640, 480, quarter_turns=0) == (640, 480)   # never upscales
