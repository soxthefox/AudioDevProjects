"""Per-hop audio features for moshviz: band energies, onset strength, polyphony.

Two analysis windows run at the same hop (see docs/decisions.md, D6):

* a short window (default 512 samples, about 10.7 ms at 48 kHz) for onset
  detection, where latency matters most;
* a long window (default 2048 samples, about 43 ms) for band energies and
  YIN. A 512-sample frame cannot resolve guitar fundamentals: the low E
  string is 82 Hz (period 582 samples), longer than the whole frame, and
  the FFT bin spacing would be 94 Hz.

:class:`FeatureExtractor` is streaming: feed it blocks of ``hop`` samples
and it returns one :class:`Features` per block.
"""

from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np
from numpy.typing import NDArray

from yin import yin

FloatArray = NDArray[np.float64]

_EPS = 1e-12


# --------------------------------------------------------------------------
# Configuration
# --------------------------------------------------------------------------

@dataclass(frozen=True)
class FeatureConfig:
    """Analysis settings. Defaults target a guitar at 48 kHz."""

    sample_rate: float = 48_000.0
    hop: int = 256
    """Samples per update. 256 at 48 kHz is about 5.3 ms."""
    onset_window: int = 512
    long_window: int = 2048
    """Used for band energies and YIN. Matches the multivoicer frame size."""

    n_bands: int = 24
    """Number of log-spaced bands, which is also the number of row strips."""
    band_fmin_hz: float = 80.0
    band_fmax_hz: float = 5_000.0
    band_min_width_bins: float = 2.0
    """Low log bands are narrower than one FFT bin; widen them to this many bins."""

    band_range_db: float = 48.0
    """Dynamic range mapped to 0..1 below the running spectral peak."""
    level_peak_release_s: float = 4.0
    """How fast the running peak level falls back after a loud passage."""

    band_attack_s: float = 0.010
    band_release_s: float = 0.250
    """Strip envelopes rise fast and fall slowly so strips do not flicker."""

    onset_compression: float = 100.0
    """Gamma in log(1 + gamma * |X|) before taking the spectral flux (Müller, FMP 6.1)."""
    onset_peak_release_s: float = 2.0
    onset_floor: float = 1e-3
    """Lower bound on the onset normaliser so silence noise is not amplified to 1."""

    yin_fmin_hz: float = 60.0
    yin_fmax_hz: float = 2_000.0
    yin_threshold: float = 0.15
    polyphony_cmndf_floor: float = 0.05
    polyphony_cmndf_ceiling: float = 0.30
    """CMNDF minimum mapped linearly to polyphony 0..1 between floor and ceiling.

    On synthetic signals (tests/test_features.py), single notes measure <= 0.02
    and chords 0.17 to 0.64. Re-fit on real guitar takes with evaluate.py.
    """
    polyphony_attack_s: float = 0.030
    polyphony_release_s: float = 0.300

    gate_db: float = -60.0
    """Below this frame RMS (dBFS) the input counts as silence: polyphony is 0."""

    @property
    def hop_s(self) -> float:
        return self.hop / self.sample_rate


# --------------------------------------------------------------------------
# Pure DSP functions
# --------------------------------------------------------------------------

def log_band_edges(n_bands: int, fmin_hz: float, fmax_hz: float) -> FloatArray:
    """Edges of ``n_bands`` bands spaced evenly in log frequency (``n_bands + 1`` values)."""
    return np.geomspace(fmin_hz, fmax_hz, n_bands + 1)


def band_weight_matrix(edges_hz: FloatArray, n_fft: int, sample_rate: float,
                       min_width_bins: float) -> FloatArray:
    """Rectangular band filters as an (n_bands, n_fft // 2 + 1) weight matrix.

    A bin belongs to a band when its centre frequency lies inside the band.
    Bands narrower than ``min_width_bins`` FFT bins are widened symmetrically
    around their geometric centre, so every band contains at least one bin.
    Neighbouring low bands can then overlap, which is acceptable here
    because adjacent strips should move together anyway. Each row is
    normalised to sum to 1, so a band's value is the mean power in it.
    """
    bin_hz = sample_rate / n_fft
    freqs = np.arange(n_fft // 2 + 1) * bin_hz
    min_width = min_width_bins * bin_hz
    weights = np.zeros((edges_hz.size - 1, freqs.size))
    for b, (lo, hi) in enumerate(zip(edges_hz[:-1], edges_hz[1:])):
        if hi - lo < min_width:
            centre = np.sqrt(lo * hi)
            lo, hi = centre - min_width / 2, centre + min_width / 2
        weights[b] = (freqs >= lo) & (freqs < hi)
    return weights / np.maximum(weights.sum(axis=1, keepdims=True), _EPS)


def power_spectrum(frame: FloatArray, window: FloatArray) -> FloatArray:
    """|FFT(frame * window)|^2, one-sided."""
    return np.abs(np.fft.rfft(frame * window)) ** 2


def spectral_flux(log_mag: FloatArray, prev_log_mag: FloatArray) -> float:
    """Half-wave rectified spectral flux: sum of bin-wise magnitude increases.

    Inputs should already be log-compressed so that quiet partials count too.
    """
    return float(np.sum(np.maximum(log_mag - prev_log_mag, 0.0)))


def spectral_flatness(power: FloatArray) -> float:
    """Geometric mean over arithmetic mean of a power spectrum, in [0, 1].

    0 for a pure tone, 1 for white noise. Computed for comparison only; see
    docs/decisions.md D1 for why it is not the polyphony measure.
    """
    p = power + _EPS
    return float(np.exp(np.mean(np.log(p))) / np.mean(p))


def spectral_peak_count(power: FloatArray, below_max_db: float = 30.0) -> int:
    """Number of local maxima within ``below_max_db`` of the strongest bin."""
    db = 10.0 * np.log10(power + _EPS)
    inner = db[1:-1]
    is_peak = (inner > db[:-2]) & (inner > db[2:]) & (inner > db.max() - below_max_db)
    return int(np.count_nonzero(is_peak))


def polyphony_from_cmndf(cmndf_min: float, floor: float, ceiling: float) -> float:
    """Map the YIN CMNDF minimum linearly to a 0..1 polyphony score."""
    return float(np.clip((cmndf_min - floor) / (ceiling - floor), 0.0, 1.0))


def rms_db(frame: FloatArray) -> float:
    return float(10.0 * np.log10(np.mean(frame * frame) + _EPS))


def one_pole_coefficient(time_constant_s: float, update_period_s: float) -> float:
    """Smoothing coefficient of a one-pole filter that moves 1 - 1/e of the way in ``time_constant_s``."""
    if time_constant_s <= 0.0:
        return 0.0
    return float(np.exp(-update_period_s / time_constant_s))


# --------------------------------------------------------------------------
# Stateful helpers
# --------------------------------------------------------------------------

class AttackRelease:
    """Per-element attack/release envelope follower (vectorised over bands)."""

    def __init__(self, size: int, attack_s: float, release_s: float, update_period_s: float):
        self._attack = one_pole_coefficient(attack_s, update_period_s)
        self._release = one_pole_coefficient(release_s, update_period_s)
        self.value = np.zeros(size)

    def process(self, target: FloatArray | float) -> FloatArray:
        target = np.asarray(target, dtype=np.float64)
        coeff = np.where(target > self.value, self._attack, self._release)
        self.value = target + (self.value - target) * coeff
        return self.value


class PeakTracker:
    """Running maximum that jumps up immediately and decays exponentially."""

    def __init__(self, release_s: float, update_period_s: float, initial: float):
        self._release = one_pole_coefficient(release_s, update_period_s)
        self.value = initial

    def process(self, x: float) -> float:
        self.value = max(x, self.value * self._release)
        return self.value


# --------------------------------------------------------------------------
# Streaming extractor
# --------------------------------------------------------------------------

@dataclass
class Features:
    """Features for one hop. Arrays have length ``FeatureConfig.n_bands``."""

    bands: FloatArray
    """Smoothed band levels in 0..1 (0 = band_range_db below the running peak)."""
    onset: float
    """Normalised spectral flux in 0..1."""
    polyphony: float
    """Smoothed polyphony in 0..1 (0 = single note, 1 = dense chord)."""
    polyphony_raw: float
    cmndf_min: float
    pitch_hz: float
    level_db: float
    flatness: float
    peak_count: int
    raw_bands: FloatArray = field(repr=False)


class FeatureExtractor:
    """Streaming feature extraction. Call :meth:`process` once per hop-sized block."""

    def __init__(self, config: FeatureConfig = FeatureConfig()):
        if config.long_window < config.onset_window or config.onset_window < config.hop:
            raise ValueError("windows must satisfy hop <= onset_window <= long_window")
        self.config = config
        c = config
        self._buffer = np.zeros(c.long_window)
        self._onset_window = np.hanning(c.onset_window)
        self._long_window = np.hanning(c.long_window)
        self._band_weights = band_weight_matrix(
            log_band_edges(c.n_bands, c.band_fmin_hz, c.band_fmax_hz),
            c.long_window, c.sample_rate, c.band_min_width_bins)
        freqs = np.fft.rfftfreq(c.long_window, 1.0 / c.sample_rate)
        self._guitar_bins = (freqs >= c.band_fmin_hz) & (freqs <= c.band_fmax_hz)

        self._prev_log_mag = np.zeros(c.onset_window // 2 + 1)
        self._onset_peak = PeakTracker(c.onset_peak_release_s, c.hop_s, 0.0)
        self._level_peak = PeakTracker(c.level_peak_release_s, c.hop_s, -np.inf)
        self._band_env = AttackRelease(c.n_bands, c.band_attack_s, c.band_release_s, c.hop_s)
        self._poly_env = AttackRelease(1, c.polyphony_attack_s, c.polyphony_release_s, c.hop_s)

    def process(self, block: FloatArray) -> Features:
        c = self.config
        if len(block) != c.hop:
            raise ValueError(f"expected a block of {c.hop} samples, got {len(block)}")
        self._buffer = np.concatenate((self._buffer[c.hop:], block))
        long_frame = self._buffer
        onset_frame = self._buffer[-c.onset_window:]

        onset = self._onset(onset_frame)

        power = power_spectrum(long_frame, self._long_window)
        band_db = 10.0 * np.log10(self._band_weights @ power + _EPS)
        peak_db = self._level_peak.process(float(band_db.max()))
        raw_bands = np.clip((band_db - (peak_db - c.band_range_db)) / c.band_range_db, 0.0, 1.0)
        bands = self._band_env.process(raw_bands).copy()

        level = rms_db(long_frame)
        result = yin(long_frame, c.sample_rate, c.yin_fmin_hz, c.yin_fmax_hz, c.yin_threshold)
        if level < c.gate_db:
            # Silence makes the CMNDF degenerate (0/0), so do not trust it.
            poly_raw = 0.0
        else:
            poly_raw = polyphony_from_cmndf(result.cmndf_min, c.polyphony_cmndf_floor,
                                            c.polyphony_cmndf_ceiling)
        polyphony = float(self._poly_env.process(poly_raw)[0])

        guitar_power = power[self._guitar_bins]
        return Features(
            bands=bands, onset=onset, polyphony=polyphony, polyphony_raw=poly_raw,
            cmndf_min=result.cmndf_min, pitch_hz=result.freq_hz if result.voiced else 0.0,
            level_db=level, flatness=spectral_flatness(guitar_power),
            peak_count=spectral_peak_count(guitar_power), raw_bands=raw_bands)

    def _onset(self, frame: FloatArray) -> float:
        c = self.config
        log_mag = np.log1p(c.onset_compression * np.abs(np.fft.rfft(frame * self._onset_window)))
        flux = spectral_flux(log_mag, self._prev_log_mag)
        self._prev_log_mag = log_mag
        normaliser = max(self._onset_peak.process(flux), c.onset_floor)
        return float(np.clip(flux / normaliser, 0.0, 1.0))


def analyse(signal: FloatArray, config: FeatureConfig = FeatureConfig()) -> list[Features]:
    """Run a fresh extractor over a whole signal (trailing partial hop is dropped)."""
    extractor = FeatureExtractor(config)
    n_hops = len(signal) // config.hop
    return [extractor.process(signal[i * config.hop:(i + 1) * config.hop]) for i in range(n_hops)]
