"""Tests for the YIN port (yin.py)."""

import numpy as np
import pytest

import synth
from yin import cmndf, difference_function, lag_range, parabolic_interpolation, yin

SR = 48_000
FRAME = 2048


def direct_difference(frame: np.ndarray, max_lag: int) -> np.ndarray:
    """The double loop from YinPitchDetector.cpp, used as the reference."""
    n = len(frame)
    d = np.zeros(max_lag + 1)
    for tau in range(1, max_lag + 1):
        diff = frame[: n - tau] - frame[tau:]
        d[tau] = np.dot(diff, diff)
    return d


def test_fft_difference_matches_direct_loop():
    rng = np.random.default_rng(1)
    frame = rng.standard_normal(FRAME)
    _, max_lag = lag_range(SR, FRAME, 60.0, 2000.0)
    np.testing.assert_allclose(difference_function(frame, max_lag),
                               direct_difference(frame, max_lag), rtol=1e-9, atol=1e-6)


def test_cmndf_starts_at_one():
    d = np.array([0.0, 2.0, 1.0, 4.0])
    out = cmndf(d)
    assert out[0] == 1.0
    np.testing.assert_allclose(out[1:], [2.0 * 1 / 2.0, 1.0 * 2 / 3.0, 4.0 * 3 / 7.0])


def test_parabolic_interpolation_finds_vertex():
    # Samples of (x - 10.3)^2 around x = 10: the vertex is at 10.3.
    xs = np.arange(20, dtype=float)
    assert parabolic_interpolation((xs - 10.3) ** 2, 10) == pytest.approx(10.3)


@pytest.mark.parametrize("freq", [82.41, 110.0, 196.0, 329.63, 659.26, 1318.5])
def test_detects_sine_pitch(freq):
    frame = synth.sine(freq, FRAME / SR, SR)
    r = yin(frame, SR)
    assert r.voiced
    assert r.freq_hz == pytest.approx(freq, rel=0.005)


@pytest.mark.parametrize("note", [40, 45, 50, 55, 59, 64])  # open strings
def test_detects_plucked_string_pitch(note):
    freq = synth.midi_to_hz(note)
    signal = synth.karplus_strong(freq, 0.5, SR)
    r = yin(signal[4800:4800 + FRAME], SR)  # skip the 100 ms noise burst
    assert r.voiced
    # Karplus-Strong rounds the period to whole samples and its lowpass adds
    # half a sample of delay, so allow about a quarter of a semitone.
    assert r.freq_hz == pytest.approx(freq, rel=0.015)


def test_silence_is_unvoiced_noise_is_unvoiced():
    rng = np.random.default_rng(2)
    assert not yin(rng.standard_normal(FRAME), SR).voiced
