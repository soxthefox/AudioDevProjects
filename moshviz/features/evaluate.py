"""Compare polyphony measures on labelled guitar recordings (decision log D1).

Record short takes and name them by content, e.g. ``single_*.wav`` for
single-note lines and ``chord_*.wav`` for strummed chords. Then run:

    python evaluate.py recordings/

For each measure (YIN CMNDF minimum, spectral flatness, spectral peak
count), the script prints per-file medians and a separation score: the
fraction of (single frame, chord frame) pairs ordered correctly. This is
the area under the ROC curve: 1.0 is perfect, 0.5 is chance. Frames
quieter than the gate are skipped.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np

from features import FeatureConfig, analyse

MEASURES = ("cmndf_min", "flatness", "peak_count")
LABELS = ("single", "chord")


def separation(single: np.ndarray, chord: np.ndarray) -> float:
    """Probability that a random chord frame scores above a random single-note frame (ties count half)."""
    diff = chord[:, None] - single[None, :]
    return float(np.mean(diff > 0) + 0.5 * np.mean(diff == 0))


def main() -> None:
    import soundfile as sf

    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("folder", type=Path)
    args = parser.parse_args()

    pooled: dict[str, dict[str, list[float]]] = {label: {m: [] for m in MEASURES} for label in LABELS}
    print(f"{'file':32s}" + "".join(f"{m:>12s}" for m in MEASURES))
    for path in sorted(args.folder.glob("*.wav")):
        label = next((lab for lab in LABELS if path.name.startswith(lab)), None)
        if label is None:
            continue
        audio, sr = sf.read(path, dtype="float64", always_2d=True)
        config = FeatureConfig(sample_rate=float(sr))
        frames = [f for f in analyse(audio.mean(axis=1), config) if f.level_db > config.gate_db]
        medians = []
        for m in MEASURES:
            values = [float(getattr(f, m)) for f in frames]
            pooled[label][m].extend(values)
            medians.append(np.median(values) if values else float("nan"))
        print(f"{path.name:32s}" + "".join(f"{v:12.3f}" for v in medians))

    if not all(pooled[lab][MEASURES[0]] for lab in LABELS):
        raise SystemExit("need at least one single_*.wav and one chord_*.wav with audible frames")
    print("\nseparation (AUC, 1.0 = perfect, 0.5 = chance):")
    for m in MEASURES:
        auc = separation(np.array(pooled["single"][m]), np.array(pooled["chord"][m]))
        print(f"  {m:12s} {auc:.3f}")


if __name__ == "__main__":
    main()
