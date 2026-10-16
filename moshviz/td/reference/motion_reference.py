"""CPU mirror of td/shaders/motion.glsl, for testing the maths outside TouchDesigner.

Keep this in step with the shader: same sampling grid, same central
differences, same regularised 2x2 Lucas-Kanade solve. Images are 2-D luma
arrays indexed [y, x]; sampling is bilinear with clamped edges, like a
TouchDesigner texture lookup.
"""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

FloatArray = NDArray[np.float64]

SAMPLES_PER_AXIS = 4


def bilinear(img: FloatArray, x: float, y: float) -> float:
    """Sample at continuous pixel coordinates, where pixel centres are at integer + 0.5."""
    h, w = img.shape
    x, y = np.clip(x - 0.5, 0, w - 1), np.clip(y - 0.5, 0, h - 1)
    x0, y0 = int(np.floor(x)), int(np.floor(y))
    x1, y1 = min(x0 + 1, w - 1), min(y0 + 1, h - 1)
    fx, fy = x - x0, y - y0
    top = img[y0, x0] * (1 - fx) + img[y0, x1] * fx
    bottom = img[y1, x0] * (1 - fx) + img[y1, x1] * fx
    return float(top * (1 - fy) + bottom * fy)


def block_motion(current: FloatArray, previous: FloatArray, block_px: int,
                 regularisation: float = 1e-3) -> FloatArray:
    """Per-block (vx, vy) in pixels/frame, shape (rows, cols, 2)."""
    h, w = current.shape
    rows, cols = h // block_px, w // block_px
    spacing = block_px / SAMPLES_PER_AXIS
    out = np.zeros((rows, cols, 2))
    for by in range(rows):
        for bx in range(cols):
            sxx = sxy = syy = sxt = syt = 0.0
            for j in range(SAMPLES_PER_AXIS):
                for i in range(SAMPLES_PER_AXIS):
                    x = bx * block_px + (i + 0.5) * spacing
                    y = by * block_px + (j + 0.5) * spacing
                    ix = (bilinear(current, x + spacing, y) - bilinear(current, x - spacing, y)) / (2 * spacing)
                    iy = (bilinear(current, x, y + spacing) - bilinear(current, x, y - spacing)) / (2 * spacing)
                    it = bilinear(current, x, y) - bilinear(previous, x, y)
                    sxx += ix * ix; sxy += ix * iy; syy += iy * iy
                    sxt += ix * it; syt += iy * it
            a, d = sxx + regularisation, syy + regularisation
            det = a * d - sxy * sxy
            out[by, bx] = -np.array([d * sxt - sxy * syt, a * syt - sxy * sxt]) / det
    return out
