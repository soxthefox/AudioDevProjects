"""Tests for features.py using synthetic signals (handover section 5)."""

import numpy as np
import pytest

import synth
from features import (AttackRelease, FeatureConfig, analyse, band_weight_matrix,
                      log_band_edges, polyphony_from_cmndf, spectral_flux)

CFG = FeatureConfig()
SR = int(CFG.sample_rate)
DURATION_S = 1.0
SETTLE_S = 0.15  # ignore the first frames: buffer filling and pluck noise burst


def steady(features, key):
    start = int(SETTLE_S / CFG.hop_s)
    return np.array([getattr(f, key) for f in features[start:]])


# --- Polyphony -------------------------------------------------------------

SINGLE_NOTES = {
    "sine 110 Hz": lambda: synth.sine(110.0, DURATION_S, SR),
    "sine 440 Hz": lambda: synth.sine(440.0, DURATION_S, SR),
    "harmonic A2": lambda: synth.harmonic_tone(110.0, DURATION_S, SR),
    "pluck low E": lambda: synth.karplus_strong(synth.midi_to_hz(40), DURATION_S, SR),
    "pluck G3": lambda: synth.karplus_strong(synth.midi_to_hz(55), DURATION_S, SR),
    "pluck high E": lambda: synth.karplus_strong(synth.midi_to_hz(64), DURATION_S, SR),
}

CHORDS = {
    "C triad harmonic": lambda: synth.chord(synth.C_MAJOR_TRIAD, DURATION_S, SR),
    "C triad pluck": lambda: synth.chord(synth.C_MAJOR_TRIAD, DURATION_S, SR, voice="pluck"),
    "E5 power chord": lambda: synth.chord((40, 47, 52), DURATION_S, SR),
    "open E harmonic": lambda: synth.chord(synth.OPEN_E_MAJOR, DURATION_S, SR),
    "open E pluck": lambda: synth.chord(synth.OPEN_E_MAJOR, DURATION_S, SR, voice="pluck"),
    "open Am pluck": lambda: synth.chord(synth.OPEN_A_MINOR, DURATION_S, SR, voice="pluck"),
    "open G pluck": lambda: synth.chord(synth.OPEN_G_MAJOR, DURATION_S, SR, voice="pluck"),
}

SINGLE_MAX_POLYPHONY = 0.1
CHORD_MIN_POLYPHONY = 0.4


@pytest.mark.parametrize("name", SINGLE_NOTES)
def test_single_note_has_low_polyphony(name):
    poly = steady(analyse(SINGLE_NOTES[name](), CFG), "polyphony_raw")
    assert np.median(poly) < SINGLE_MAX_POLYPHONY, f"{name}: median {np.median(poly):.2f}"


@pytest.mark.parametrize("name", CHORDS)
def test_chord_has_high_polyphony(name):
    poly = steady(analyse(CHORDS[name](), CFG), "polyphony_raw")
    assert np.median(poly) > CHORD_MIN_POLYPHONY, f"{name}: median {np.median(poly):.2f}"


def test_flatness_does_not_separate_notes_from_chords():
    """Evidence for decision D1: a plucked low E is as 'flat' as a plucked chord."""
    low_e = np.median(steady(analyse(SINGLE_NOTES["pluck low E"](), CFG), "flatness"))
    chord = np.median(steady(analyse(CHORDS["open E pluck"](), CFG), "flatness"))
    assert low_e > chord


def test_silence_has_zero_polyphony():
    feats = analyse(np.zeros(SR // 2), CFG)
    assert all(f.polyphony == 0.0 for f in feats)


def test_polyphony_mapping_clamps():
    assert polyphony_from_cmndf(0.0, 0.05, 0.3) == 0.0
    assert polyphony_from_cmndf(0.175, 0.05, 0.3) == pytest.approx(0.5)
    assert polyphony_from_cmndf(0.9, 0.05, 0.3) == 1.0


# --- Onsets ----------------------------------------------------------------

def test_impulse_train_produces_onset_peaks():
    period_s = 0.25
    signal, times = synth.impulse_train(period_s, 2.0, SR)
    onset = np.array([f.onset for f in analyse(signal, CFG)])
    # A hop's value describes the window ending at that hop.
    hop_end_times = (np.arange(onset.size) + 1) * CFG.hop_s
    # Peak picking: local maxima above 0.5. A click stays inside the
    # 512-sample window for two hops, so a plain threshold can fire twice.
    padded = np.r_[0.0, onset, 0.0]
    is_peak = (onset > padded[:-2]) & (onset >= padded[2:]) & (onset > 0.5)
    detected = hop_end_times[is_peak]

    assert len(detected) == len(times)
    # The click enters the 512-sample onset window within one hop.
    np.testing.assert_allclose(detected, times, atol=2 * CFG.hop_s)
    # Between clicks, the onset function falls back to ~0.
    between = onset[(hop_end_times % period_s) > 0.6 * period_s]
    assert np.max(between) < 0.1


def test_spectral_flux_is_half_wave_rectified():
    assert spectral_flux(np.array([1.0, 2.0]), np.array([3.0, 3.0])) == 0.0
    assert spectral_flux(np.array([4.0, 2.0]), np.array([3.0, 3.0])) == 1.0


def test_onset_strum_beats_sustain():
    signal = synth.chord(synth.OPEN_E_MAJOR, 1.0, SR, voice="pluck")
    padded = np.concatenate((np.zeros(SR // 4), signal))
    onset = np.array([f.onset for f in analyse(padded, CFG)])
    strum_hop = int(0.25 / CFG.hop_s)
    assert onset[strum_hop:strum_hop + 4].max() > 0.9
    assert np.median(onset[strum_hop + 40:]) < 0.2


# --- Bands -----------------------------------------------------------------

def peak_band(freq_hz: float) -> int:
    feats = analyse(synth.sine(freq_hz, 0.5, SR), CFG)
    return int(np.argmax(np.mean(steady(feats, "bands"), axis=0)))


def test_110_hz_lands_in_low_strips():
    assert peak_band(110.0) < CFG.n_bands // 4


def test_2_khz_lands_in_high_strips():
    assert peak_band(2000.0) >= 3 * CFG.n_bands // 4 - 2


def test_band_index_is_monotonic_in_frequency():
    idx = [peak_band(f) for f in (100, 200, 400, 800, 1600, 3200)]
    assert idx == sorted(idx) and len(set(idx)) == len(idx)


def test_every_band_has_at_least_one_bin():
    w = band_weight_matrix(log_band_edges(32, 80, 5000), 2048, SR, min_width_bins=2.0)
    assert np.all(w.sum(axis=1) == pytest.approx(1.0))


def test_single_note_lights_fewer_bands_than_chord():
    def lit(signal):
        bands = np.mean(steady(analyse(signal, CFG), "bands"), axis=0)
        return int(np.count_nonzero(bands > 0.7))
    assert lit(synth.karplus_strong(synth.midi_to_hz(64), 1.0, SR)) < \
        lit(synth.chord(synth.OPEN_E_MAJOR, 1.0, SR, voice="pluck"))


# --- Envelope --------------------------------------------------------------

def test_attack_release_is_asymmetric():
    env = AttackRelease(1, attack_s=0.01, release_s=0.5, update_period_s=0.005)
    for _ in range(10):
        env.process(1.0)
    assert env.value[0] > 0.95  # fast attack
    for _ in range(10):
        env.process(0.0)
    assert env.value[0] > 0.8   # slow release
