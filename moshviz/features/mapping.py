"""Turn features into per-strip mosh amounts (the 1 x N row texture in TouchDesigner).

    amount[i] = clamp(gain * spread(shape(bands, p))[i] * f(p) * g(onset), 0, 1)

* shape: raises band levels to a power that is high for single notes and
  1 for chords. Only the strongest strips of a melody survive, so a melody
  moshes a few thin strips.
* spread: blurs across neighbouring strips with a width that grows with
  polyphony, so a chord moshes large connected areas.
* f(p) = melody_gain + (chord_gain - melody_gain) * p^poly_curve
* g(o) = 1 + onset_boost * o^onset_curve, a short kick on strums and plucks.

Every constant is a field of :class:`MappingParams`, so each one can be
changed live over OSC (``/moshviz/param/<name> <float>``).
"""

from __future__ import annotations

from dataclasses import dataclass, fields

import numpy as np
from numpy.typing import NDArray

FloatArray = NDArray[np.float64]


@dataclass
class MappingParams:
    gain: float = 1.0
    melody_gain: float = 0.6
    chord_gain: float = 1.0
    poly_curve: float = 1.0
    melody_sharpness: float = 4.0
    """Exponent applied to band levels when polyphony = 0 (falls to 1 at polyphony = 1)."""
    max_spread_strips: float = 3.0
    """Gaussian blur sigma across strips at polyphony = 1."""
    onset_boost: float = 0.5
    onset_curve: float = 2.0

    @classmethod
    def names(cls) -> list[str]:
        return [f.name for f in fields(cls)]

    def set(self, name: str, value: float) -> None:
        """Set a parameter by name. Raises KeyError for unknown names."""
        if name not in self.names():
            raise KeyError(name)
        setattr(self, name, float(value))


def gaussian_spread(values: FloatArray, sigma: float) -> FloatArray:
    """Blur a 1-D strip profile with a Gaussian of ``sigma`` strips (edges clamp).

    Uses a max-preserving blend: the output never drops below the input, so
    a single strong strip widens instead of fading.
    """
    if sigma < 1e-3:
        return values.copy()
    radius = int(np.ceil(3.0 * sigma))
    offsets = np.arange(-radius, radius + 1)
    kernel = np.exp(-0.5 * (offsets / sigma) ** 2)
    kernel /= kernel.max()  # peak 1, so the centre strip keeps its own value
    padded = np.pad(values, radius, mode="edge")
    # Max over (neighbour value x kernel weight) is a soft morphological dilation.
    windows = np.lib.stride_tricks.sliding_window_view(padded, offsets.size)
    return np.max(windows * kernel, axis=1)


def row_amounts(bands: FloatArray, polyphony: float, onset: float,
                params: MappingParams) -> FloatArray:
    """Compute per-strip mosh amounts in 0..1. See the module docstring for the formula."""
    p = float(np.clip(polyphony, 0.0, 1.0))
    o = float(np.clip(onset, 0.0, 1.0))

    exponent = params.melody_sharpness + (1.0 - params.melody_sharpness) * p
    shaped = np.clip(bands, 0.0, 1.0) ** max(exponent, 1e-3)
    spread = gaussian_spread(shaped, params.max_spread_strips * p)

    poly_gain = params.melody_gain + (params.chord_gain - params.melody_gain) * p ** params.poly_curve
    onset_gain = 1.0 + params.onset_boost * o ** params.onset_curve
    return np.clip(params.gain * spread * poly_gain * onset_gain, 0.0, 1.0)
