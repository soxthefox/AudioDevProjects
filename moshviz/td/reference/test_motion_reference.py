"""Checks the motion-pass maths (and its sign convention) on known shifts."""

import numpy as np
import pytest

from motion_reference import block_motion

SIZE = 64
BLOCK = 8
# Smooth texture: sums of low-frequency sinusoids, so LK's linearisation holds.
PERIOD_X, PERIOD_Y = 23.0, 17.0


def texture(shift_x: float = 0.0, shift_y: float = 0.0) -> np.ndarray:
    """Pattern translated by (shift_x, shift_y) pixels, i.e. content moved right/down."""
    y, x = np.mgrid[0:SIZE, 0:SIZE] + 0.5
    x, y = x - shift_x, y - shift_y
    return 0.5 + 0.25 * np.sin(2 * np.pi * x / PERIOD_X) * np.cos(2 * np.pi * y / PERIOD_Y) \
        + 0.1 * np.sin(2 * np.pi * (x + y) / 31.0)


def interior(field: np.ndarray) -> np.ndarray:
    return field[1:-1, 1:-1]  # edge blocks see clamped samples


@pytest.mark.parametrize("shift", [(1.0, 0.0), (0.0, -1.0), (0.7, 0.5), (-1.5, 1.0)])
def test_recovers_translation(shift):
    v = interior(block_motion(texture(*shift), texture(), BLOCK, regularisation=1e-6))
    median = np.median(v.reshape(-1, 2), axis=0)
    np.testing.assert_allclose(median, shift, atol=0.15)


def test_static_scene_has_zero_motion():
    v = block_motion(texture(), texture(), BLOCK)
    assert np.max(np.abs(v)) < 1e-9


def test_flat_region_stays_near_zero():
    flat = np.full((SIZE, SIZE), 0.5)
    v = block_motion(flat + 0.01, flat, BLOCK)  # brightness change, no texture
    assert np.max(np.abs(v)) < 1e-9
