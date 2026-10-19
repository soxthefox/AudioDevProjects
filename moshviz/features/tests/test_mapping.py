"""Tests for mapping.py."""

import numpy as np
import pytest

from mapping import MappingParams, apply_threshold, gaussian_spread, level_gate, row_amounts

N = 24


def one_hot(i: int, level: float = 1.0) -> np.ndarray:
    v = np.zeros(N)
    v[i] = level
    return v


def test_spread_widens_without_lowering_peak():
    out = gaussian_spread(one_hot(10), sigma=2.0)
    assert out[10] == pytest.approx(1.0)
    assert out[8] > 0.5 and out[12] > 0.5
    assert out[0] < 1e-3


def test_zero_sigma_is_identity():
    v = np.random.default_rng(0).random(N)
    np.testing.assert_array_equal(gaussian_spread(v, 0.0), v)


def test_melody_mosh_fewer_strips_than_chord():
    params = MappingParams()
    bands = np.full(N, 0.5)
    bands[[4, 9, 14]] = 1.0  # a note: fundamental + harmonics strong
    melody = row_amounts(bands, polyphony=0.0, onset=0.0, params=params)
    chord = row_amounts(bands, polyphony=1.0, onset=0.0, params=params)
    assert np.count_nonzero(melody > 0.3) <= 3
    assert np.count_nonzero(chord > 0.3) > 3 * np.count_nonzero(melody > 0.3)


def test_onset_boosts_amounts():
    params = MappingParams()
    bands = np.full(N, 0.5)
    quiet = row_amounts(bands, 0.5, 0.0, params)
    hit = row_amounts(bands, 0.5, 1.0, params)
    assert np.all(hit >= quiet) and hit.sum() > quiet.sum()


def test_amounts_stay_in_unit_range():
    params = MappingParams(gain=10.0)
    out = row_amounts(np.ones(N), 1.0, 1.0, params)
    assert out.min() >= 0.0 and out.max() <= 1.0


def test_set_param_by_name():
    params = MappingParams()
    params.set("chord_gain", 0.25)
    assert params.chord_gain == 0.25
    with pytest.raises(KeyError):
        params.set("nope", 1.0)


def test_gate_closes_below_gate_level():
    params = MappingParams(gate_db=-50.0, gate_knee_db=10.0)
    bands = np.ones(N)
    assert row_amounts(bands, 1.0, 0.0, params, level_db=-60.0).max() == 0.0
    assert row_amounts(bands, 1.0, 0.0, params, level_db=-20.0).max() > 0.5


def test_level_gate_knee_is_linear():
    assert level_gate(-50.0, -50.0, 10.0) == 0.0
    assert level_gate(-45.0, -50.0, 10.0) == pytest.approx(0.5)
    assert level_gate(-30.0, -50.0, 10.0) == 1.0


def test_threshold_zeroes_weak_bands_and_rescales():
    out = apply_threshold(np.array([0.2, 0.4, 0.7, 1.0]), 0.4)
    np.testing.assert_allclose(out, [0.0, 0.0, 0.5, 1.0])
