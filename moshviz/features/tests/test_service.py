"""Tests for service.py helpers (no audio hardware needed)."""

import numpy as np
import soundfile as sf
from pythonosc.osc_bundle import OscBundle

import synth
from features import FeatureConfig, FeatureExtractor
from mapping import MappingParams, row_amounts
from service import build_bundle, load_audio

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
