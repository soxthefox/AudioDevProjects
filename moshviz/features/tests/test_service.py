"""Tests for service.py helpers (no audio hardware needed)."""

import numpy as np
import soundfile as sf
from pythonosc.osc_bundle import OscBundle

import synth
from features import FeatureConfig, FeatureExtractor
from mapping import MappingParams, row_amounts
from service import ClipWindow, build_bundle, load_audio

SR = 48_000


def test_bundle_carries_rows_scalars_and_time():
    config = FeatureConfig()
    features = FeatureExtractor(config).process(np.zeros(config.hop))
    rows = row_amounts(features.bands, features.polyphony, features.onset, MappingParams())
    bundle = OscBundle(build_bundle(features, rows, seq=7, time_s=1.25).dgram)
    messages = {m.address: m.params[0] for m in bundle}
    assert sum(a.startswith('/moshviz/row/') for a in messages) == config.n_bands
    assert messages['/moshviz/seq'] == 7
    assert messages['/moshviz/time'] == 1.25


def test_load_audio_reads_wav(tmp_path):
    path = tmp_path / 'tone.wav'
    tone = synth.sine(440.0, 0.5, SR)
    sf.write(path, np.stack([tone, tone], axis=1), SR)
    audio = load_audio(path, SR)
    assert audio.shape == (len(tone), 2)
    np.testing.assert_allclose(audio[:, 0], tone, atol=1e-4)


def test_clip_window_defaults_to_whole_file():
    assert ClipWindow().bounds(48_000, SR, min_frames=256) == (0, 48_000)


def test_clip_window_trims_and_clamps():
    clip = ClipWindow()
    clip.set('start', 0.25)
    clip.set('end', 0.5)
    assert clip.bounds(48_000, SR, min_frames=256) == (12_000, 24_000)
    clip.set('end', 0.0)   # 0 = to the end of the file
    assert clip.bounds(48_000, SR, min_frames=256) == (12_000, 48_000)
    clip.set('start', 5.0)  # past the end: keep at least one block
    assert clip.bounds(48_000, SR, min_frames=256) == (47_744, 48_000)
