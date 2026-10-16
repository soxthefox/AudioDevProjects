"""Synthetic test signals: sines, harmonic tones, plucked strings, chords.

Used by the unit tests and by ``evaluate.py``. Everything is deterministic
when a seed is given.
"""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

FloatArray = NDArray[np.float64]

A4_HZ = 440.0
A4_MIDI = 69
SEMITONES_PER_OCTAVE = 12

# Standard-tuning open-position chord voicings, as MIDI notes (low to high).
OPEN_E_MAJOR = (40, 47, 52, 56, 59, 64)   # E2 B2 E3 G#3 B3 E4
OPEN_A_MINOR = (45, 52, 57, 60, 64)       # A2 E3 A3 C4 E4
OPEN_G_MAJOR = (43, 47, 50, 55, 59, 67)   # G2 B2 D3 G3 B3 G4
C_MAJOR_TRIAD = (48, 52, 55)              # C3 E3 G3


def midi_to_hz(note: float) -> float:
    """Equal-tempered frequency of a MIDI note number (A4 = 440 Hz)."""
    return A4_HZ * 2.0 ** ((note - A4_MIDI) / SEMITONES_PER_OCTAVE)


def time_axis(duration_s: float, sample_rate: float) -> FloatArray:
    return np.arange(int(round(duration_s * sample_rate))) / sample_rate


def sine(freq_hz: float, duration_s: float, sample_rate: float,
         amplitude: float = 0.5) -> FloatArray:
    return amplitude * np.sin(2.0 * np.pi * freq_hz * time_axis(duration_s, sample_rate))


def harmonic_tone(freq_hz: float, duration_s: float, sample_rate: float,
                  n_harmonics: int = 8, rolloff: float = 1.0,
                  amplitude: float = 0.5, phase_seed: int | None = 0) -> FloatArray:
    """Sum of harmonics k*f0 with amplitude 1/k^rolloff and random phases.

    Harmonics above Nyquist are dropped. The result is peak-normalised to
    ``amplitude``.
    """
    rng = np.random.default_rng(phase_seed)
    t = time_axis(duration_s, sample_rate)
    out = np.zeros_like(t)
    for k in range(1, n_harmonics + 1):
        if k * freq_hz >= sample_rate / 2:
            break
        out += np.sin(2.0 * np.pi * k * freq_hz * t + rng.uniform(0, 2 * np.pi)) / k**rolloff
    return amplitude * out / max(np.max(np.abs(out)), 1e-12)


def karplus_strong(freq_hz: float, duration_s: float, sample_rate: float,
                   decay: float = 0.996, amplitude: float = 0.5,
                   seed: int | None = 0) -> FloatArray:
    """Karplus-Strong plucked string: a noise burst in a lowpassed delay loop.

    Args:
        decay: Loop gain per period (< 1). Closer to 1 rings longer.
    """
    rng = np.random.default_rng(seed)
    period = max(2, int(round(sample_rate / freq_hz)))
    n = int(round(duration_s * sample_rate))
    out = np.zeros(n)
    out[:period] = rng.uniform(-1.0, 1.0, min(period, n))
    for i in range(period, n):
        # Two-point average is the classic KS lowpass.
        out[i] = decay * 0.5 * (out[i - period] + out[i - period - 1 if i - period - 1 >= 0 else 0])
    out -= np.mean(out)
    return amplitude * out / max(np.max(np.abs(out)), 1e-12)


def chord(midi_notes: tuple[int, ...], duration_s: float, sample_rate: float,
          voice: str = "harmonic", amplitude: float = 0.5,
          seed: int = 0) -> FloatArray:
    """Sum of one voice per note, peak-normalised.

    Args:
        voice: "harmonic" (``harmonic_tone``) or "pluck" (``karplus_strong``).
    """
    out = np.zeros(int(round(duration_s * sample_rate)))
    for i, note in enumerate(midi_notes):
        f = midi_to_hz(note)
        if voice == "harmonic":
            out += harmonic_tone(f, duration_s, sample_rate, phase_seed=seed + i)
        elif voice == "pluck":
            out += karplus_strong(f, duration_s, sample_rate, seed=seed + i)
        else:
            raise ValueError(f"unknown voice {voice!r}")
    return amplitude * out / max(np.max(np.abs(out)), 1e-12)


def impulse_train(period_s: float, duration_s: float, sample_rate: float,
                  amplitude: float = 0.9) -> tuple[FloatArray, FloatArray]:
    """Clicks every ``period_s`` seconds, starting at ``period_s / 2``.

    Returns:
        (signal, impulse_times_s)
    """
    n = int(round(duration_s * sample_rate))
    out = np.zeros(n)
    times = np.arange(period_s / 2, duration_s, period_s)
    out[np.round(times * sample_rate).astype(int)] = amplitude
    return out, times
