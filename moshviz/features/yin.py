"""YIN pitch detection, ported from the multivoicer JUCE plugin.

Source: ``multivoicer/Source/Dsp/YinPitchDetector.cpp`` on branch
``feat/multivoicer-v1`` of soxthefox/AudioDevProjects. The four steps
(difference function, CMNDF, absolute threshold, parabolic interpolation)
follow the C++ code one to one. Two things are new here:

* The difference function is computed with an FFT autocorrelation instead
  of the O(N * maxLag) double loop. ``tests/test_yin.py`` checks that it
  matches the direct loop.
* :class:`YinResult` also returns ``cmndf_min``, the global minimum of the
  CMNDF over the search range. moshviz uses it as a polyphony measure: a
  single note has one dominant period, so the CMNDF dips close to zero; a
  chord has no common period inside the search range, so the minimum stays
  high.

Reference: A. de Cheveigné and H. Kawahara, "YIN, a fundamental frequency
estimator for speech and music", JASA 111(4), 2002.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from numpy.typing import NDArray

FloatArray = NDArray[np.float64]

# Guards against division by zero in the CMNDF and the parabola fit. Same
# value as the C++ implementation.
_EPS = 1e-12


@dataclass(frozen=True)
class YinResult:
    """Outcome of one YIN analysis frame."""

    freq_hz: float
    """Estimated fundamental, or 0.0 when no lag passed the threshold."""
    confidence: float
    """1 - CMNDF at the chosen lag, clamped to [0, 1] (as in the C++ code)."""
    voiced: bool
    """True when a pitch was found inside [fmin, fmax] with confidence >= 0.5."""
    cmndf_min: float
    """Global CMNDF minimum over the searched lags (moshviz polyphony input)."""


def difference_function(frame: FloatArray, max_lag: int) -> FloatArray:
    """YIN step 1: d(tau) = sum_{i=0}^{N-tau-1} (x[i] - x[i+tau])^2.

    Matches the C++ loop, whose sum length shrinks as tau grows. Expanding
    the square gives two energy terms (prefix sums) and one cross term (the
    autocorrelation r(tau), computed via FFT).

    Args:
        frame: Mono analysis frame of length N.
        max_lag: Largest lag to evaluate. Must be < N.

    Returns:
        Array of length ``max_lag + 1``; index 0 is 0 by definition.
    """
    x = np.asarray(frame, dtype=np.float64)
    n = x.size
    if not 0 < max_lag < n:
        raise ValueError(f"max_lag must be in (0, {n}), got {max_lag}")

    # Linear (not circular) autocorrelation: zero-pad to at least 2N.
    fft_size = 1 << int(np.ceil(np.log2(2 * n)))
    spectrum = np.fft.rfft(x, fft_size)
    autocorr = np.fft.irfft(spectrum * np.conj(spectrum), fft_size)[: max_lag + 1]

    squared = x * x
    prefix = np.concatenate(([0.0], np.cumsum(squared)))  # prefix[k] = sum x[:k]^2
    taus = np.arange(max_lag + 1)
    head_energy = prefix[n - taus]                # sum_{i < N-tau} x[i]^2
    tail_energy = prefix[n] - prefix[taus]        # sum_{i >= tau} x[i]^2
    d = head_energy + tail_energy - 2.0 * autocorr
    d[0] = 0.0
    # Floating-point cancellation can leave tiny negatives where d is ~0.
    return np.maximum(d, 0.0)


def cmndf(d: FloatArray) -> FloatArray:
    """YIN step 2: cumulative mean normalised difference.

    d'(0) = 1, d'(tau) = d(tau) * tau / sum_{j=1}^{tau} d(j).
    """
    out = np.ones_like(d)
    running = np.cumsum(d[1:])
    taus = np.arange(1, d.size)
    out[1:] = d[1:] * taus / np.maximum(running, _EPS)
    return out


def absolute_threshold(norm_diff: FloatArray, min_lag: int, max_lag: int,
                       threshold: float) -> int:
    """YIN step 3: first lag below ``threshold``, walked down to its local minimum.

    Returns:
        The chosen lag, or -1 if no lag in [min_lag, max_lag] is below threshold.
    """
    for tau in range(min_lag, max_lag + 1):
        if norm_diff[tau] < threshold:
            while tau + 1 <= max_lag and norm_diff[tau + 1] < norm_diff[tau]:
                tau += 1
            return tau
    return -1


def parabolic_interpolation(norm_diff: FloatArray, tau: int) -> float:
    """YIN step 4: refine an integer lag by fitting a parabola through 3 points."""
    if not 0 < tau < norm_diff.size - 1:
        return float(tau)
    s0, s1, s2 = norm_diff[tau - 1], norm_diff[tau], norm_diff[tau + 1]
    denom = 2.0 * (2.0 * s1 - s2 - s0)
    if abs(denom) <= _EPS:
        return float(tau)
    return tau + (s2 - s0) / denom


def lag_range(sample_rate: float, frame_size: int, fmin_hz: float,
              fmax_hz: float) -> tuple[int, int]:
    """Lag search range [min_lag, max_lag], computed exactly as in the C++ prepare()."""
    min_lag = max(2, int(sample_rate / fmax_hz))
    max_lag = min(frame_size // 2, int(sample_rate / fmin_hz))
    return min_lag, max_lag


def yin(frame: FloatArray, sample_rate: float, fmin_hz: float = 60.0,
        fmax_hz: float = 2000.0, threshold: float = 0.15) -> YinResult:
    """Run YIN on one frame.

    Defaults (60 Hz to 2 kHz, threshold 0.15) are the multivoicer constants.

    Args:
        frame: Mono analysis frame. multivoicer uses 2048 samples.
        sample_rate: Sample rate in Hz.
        fmin_hz: Lowest fundamental searched.
        fmax_hz: Highest fundamental searched.
        threshold: CMNDF absolute threshold for step 3.
    """
    min_lag, max_lag = lag_range(sample_rate, len(frame), fmin_hz, fmax_hz)
    norm_diff = cmndf(difference_function(frame, max_lag))
    cmndf_min = float(np.min(norm_diff[min_lag:max_lag + 1]))

    tau = absolute_threshold(norm_diff, min_lag, max_lag, threshold)
    if tau < 0:
        return YinResult(freq_hz=0.0, confidence=0.0, voiced=False, cmndf_min=cmndf_min)

    freq = sample_rate / parabolic_interpolation(norm_diff, tau)
    confidence = float(np.clip(1.0 - norm_diff[tau], 0.0, 1.0))
    voiced = fmin_hz <= freq <= fmax_hz and confidence >= 0.5
    return YinResult(freq_hz=freq, confidence=confidence, voiced=voiced, cmndf_min=cmndf_min)
