# moshviz

A real-time audio-reactive datamosh in TouchDesigner. A camera feed of a
guitarist is datamoshed strip by strip in response to the live guitar.
Single melodic notes mosh a few thin strips; strummed chords mosh large
areas. The performer, segmented by depth or by a person mask, always passes
through clean.

```
guitar ─► audio interface ─► features/service.py ──OSC──► TouchDesigner
                               bands, onset, polyphony        │
camera ─► subject mask ───────────────────────────────────────┤
                                       motion.glsl (block motion vectors)
                                       mosh.glsl   (feedback datamosh) ─► out
```

## Status

| # | Milestone | State |
|---|---|---|
| 1 | Camera spike (Kinect v1 on macOS, or fallback) | not started: needs hardware ([plan](capture/README.md)) |
| 2 | Subject mask | not started |
| 3 | Feature service + tests | **code and tests done**; live OSC check in TouchDesigner pending |
| 4 | Mosh shader v1 | **written**, maths unit-tested on CPU; needs a visual check in TouchDesigner |
| 5 | Tuning with guitar | — |
| 6 | Measurements | — |
| 7 | Mini user test | — |
| 8 | Demo video | — |

## Running

```bash
cd moshviz
python3 -m venv .venv && .venv/bin/pip install -r features/requirements.txt
.venv/bin/pytest                                  # all tests
.venv/bin/python features/service.py --list-devices
.venv/bin/python features/service.py --device "<interface name>" --channel 0
.venv/bin/python features/service.py --file take.wav --loop   # no guitar needed
.venv/bin/python features/evaluate.py recordings/             # compare polyphony measures
```

TouchDesigner setup and what you should see: [td/README.md](td/README.md).

## Layout

```
features/   Python feature service (audio in, OSC out)
  yin.py        YIN pitch detector, ported from multivoicer (see below)
  features.py   band energies, spectral-flux onsets, polyphony, smoothing
  mapping.py    features → per-strip mosh amounts (all parameters live over OSC)
  service.py    real-time audio I/O and OSC
  evaluate.py   compares polyphony measures on labelled recordings
  synth.py      synthetic test signals (plucked strings, chords, clicks)
  tests/
td/
  shaders/motion.glsl   per-macroblock Lucas-Kanade motion vectors
  shaders/mosh.glsl     feedback datamosh, per-strip amount, subject mask
  reference/            numpy mirror of the motion shader + tests
capture/    Kinect / segmentation bridge (milestones 1 and 2)
docs/decisions.md       decision log
```

## How it works

**Polyphony from YIN.** The core signal is whether the guitar is playing
one note or a chord. YIN's cumulative mean normalised difference function
(CMNDF) dips close to zero at the period of a single note. A chord has no
common period in the searched range, so its minimum stays high. On
synthetic plucked strings this separates notes from chords far better than
spectral flatness or peak counting, which both mistake a low E string for
a chord ([D1](docs/decisions.md)). Real guitar takes are next.

**Reused DSP.** `features/yin.py` is a Python/numpy port of the YIN pitch
detector from my JUCE plugin *multivoicer*
(`multivoicer/Source/Dsp/YinPitchDetector.cpp`, branch
`feat/multivoicer-v1`). The algorithm steps are line for line the same.
The difference function is computed with an FFT autocorrelation instead
of the double loop, and a test checks that both agree.

**Datamosh as codec emulation.** A video codec predicts each frame by
moving blocks of the previous frame along motion vectors, and keyframes
periodically reset it. Datamoshing removes the keyframes, so stale
pixels get dragged along new motion. `motion.glsl` estimates one vector
per 8 × 8 block, as an encoder would. `mosh.glsl` then shows, per strip,
the previous output warped by those vectors instead of the live frame,
weighted by that strip's audio-driven amount.

## Measurements

To do (milestone 6): output frame rate at resolution, and audio-to-visual
latency (240 fps phone video of a hard pluck plus the screen).

Measured so far: feature extraction takes 0.35 ms per 5.3 ms hop on an
Apple M1.

## User test

To do (milestone 7).
