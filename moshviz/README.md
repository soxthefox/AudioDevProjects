# moshviz

A real-time, audio-reactive datamosh for live performance, built in
TouchDesigner with hand-written GLSL and a Python audio-analysis service.

## What we are doing

A camera films a performer, at first Sohum playing guitar. The live audio
drives a datamosh of the image: video blocks freeze and get dragged along
the performer's movement, the way a video stream breaks when it loses its
keyframes. The music decides **where** and **how much** the image breaks:

- **Frequency → place.** The spectrum is split into 24 bands, laid out as
  concentric rings: bass is a large disc at the centre of the frame, and
  the highs are thin rings out at the edges.
- **Melody vs chord → shape.** A polyphony measure (from YIN pitch
  detection) tells single notes from chords. Single notes mosh a few
  narrow bands; chords mosh wide, connected areas.
- **Onsets → kick.** Strums and drum hits give a short boost.
- **Silence → clean.** A level gate keeps the image intact when nothing is
  playing.

The performer is segmented and always stays clean while the world around
them moshes. The image is not photographic: it is turned into
lidar-like outlines, where moving edges glow and still ones fade, so the
piece is about movement rather than the room. An optional "clones" pass
fills the frame with a crowd of copies of the performer, receding in
rows to a vanishing point, each row repeating the movement a few frames
later.

**Why.** This is portfolio work for master's applications. Each part
shows a skill that the rest of the transcript does not:

| Part | Skill shown | For |
|---|---|---|
| Polyphony, onset and band features from live audio; YIN reused from my own plugin | Signal processing, music information retrieval | UPF Sound and Music Computing |
| Hand-written GLSL: motion estimation, datamosh, stylisation, clones | Real-time graphics programming | Utrecht Game and Media Technology |
| Live interactive audiovisual system, with a small user test | Interaction design and evaluation | KTH Interactive Media Technology |

The reasoning behind every significant choice is in the
[decision log](docs/decisions.md).

## Pipeline

```
 guitar / mic ──► features/service.py ──OSC──► oscin1 ─► rows_sel ─► rows (1 x 24 texture)
                  bands, onset, polyphony,                                 │
                  gate → 24 mosh amounts                                   │
                                                                           ▼
 webcam ──► MediaPipe plugin ──► mp_video ─► cam ─┬─► motion.glsl  (block motion vectors)
                     │                            │        │
                     └──► selfie mask ──► mask    │        ▼
                                           │      └─► stylize.glsl (glowing outlines)
                                           │               │
                                           └──────────► mosh.glsl   (feedback datamosh, rings)
                                                           │
                                                        look.glsl   (grade + motion glow)
                                                           │
                                                        clones.glsl (optional crowd) ─► out
```

## Status

| # | Milestone | State |
|---|---|---|
| 1 | Camera | **Webcam + MediaPipe** in use; Kinect deferred ([capture/README.md](capture/README.md)) |
| 2 | Subject mask | **Done**: MediaPipe selfie segmentation, live, aligned with the video |
| 3 | Feature service + tests | **Done**: 50 feature tests (+ 6 for the motion maths); live with the MacBook mic |
| 4 | Shaders | **Done**: motion, mosh (rings), stylize, look, clones |
| 5 | Tuning with guitar | In progress (tuned so far with music through the mic) |
| 6 | Measurements | Partly done (see below); audio-to-visual latency still to measure |
| 7 | Mini user test | To do |
| 8 | Demo video | To do |

## Running it

**1. Python feature service** (from the `moshviz` folder):

```bash
python3 -m venv .venv && .venv/bin/pip install -r features/requirements.txt
.venv/bin/pytest                                                   # 56 tests
.venv/bin/python features/service.py --list-devices
.venv/bin/python features/service.py --device "MacBook Air Microphone"
.venv/bin/python features/service.py --device "<interface>" --channel 0   # guitar
.venv/bin/python features/service.py --file take.wav --loop              # recording (silent)
.venv/bin/python features/service.py --file show.mov --play --loop        # play a video's audio as the clock
```

The service logs level, polyphony, onset and strip amounts every 2 s.

**2. MediaPipe plugin.** Download `release.zip` of
[torinmb/mediapipe-touchdesigner](https://github.com/torinmb/mediapipe-touchdesigner)
v0.6.0 and unzip it to `td/vendor/mediapipe-touchdesigner/`. That folder is
gitignored (188 MB), and the `.toe` links to it as an external `.tox`.

**3. TouchDesigner.** Open `td/moshviz.toe`. The project runs at 30 fps to
match the camera. Then, in the Textport (Dialogs → Textport and DATs):

```python
exec(open(project.folder + '/scripts/mediapipe_settings.py').read())   # segmentation only
exec(open(project.folder + '/scripts/wiring.py').read())               # check/repair all connections
exec(open(project.folder + '/scripts/uniforms.py').read())             # apply all shader values
```

To run on a video file instead of the camera (no performer mask, clones
off; the movie follows the audio clock):

```python
MOSHVIZ_SOURCE = 'movie'; exec(open(project.folder + '/scripts/wiring.py').read())
```

Live controls (clones on/off, clip trim, zoom, offset, rotation) are on
the **`controls`** node's parameter panel, on its Controls page.

Building the network from scratch, step by step: [td/WALKTHROUGH.md](td/WALKTHROUGH.md).
Compact reference: [td/README.md](td/README.md).

## Everything lives in text

TouchDesigner saves projects as binary `.toe` files, which version control
cannot diff. So the `.toe` holds as little as possible:

- **Shaders** are `.glsl` files, loaded by Text DATs with Sync to File.
- **Shader values** (every uniform) live in
  [td/scripts/uniforms.py](td/scripts/uniforms.py).
- **Connections** (every wire and operator reference) live in
  [td/scripts/wiring.py](td/scripts/wiring.py), which also repairs a broken
  network.
- **Plugin settings** live in
  [td/scripts/mediapipe_settings.py](td/scripts/mediapipe_settings.py).
- **Audio mapping parameters** live in `features/mapping.py`, and can be
  changed live over OSC (`/moshviz/param/<name>` to port 9001).

## Layout

```
features/               Python feature service (audio in, OSC out)
  yin.py                YIN pitch detector, ported from multivoicer (see below)
  features.py           band energies, spectral-flux onsets, polyphony, smoothing
  mapping.py            features → 24 mosh amounts (gate, threshold, sharpness, spread)
  service.py            real-time audio input and OSC
  evaluate.py           compares polyphony measures on labelled recordings
  synth.py              synthetic test signals (plucked strings, chords, clicks)
  tests/
td/
  moshviz.toe           TouchDesigner project (binary, minimal)
  shaders/motion.glsl   per-macroblock Lucas-Kanade motion vectors
  shaders/stylize.glsl  lidar-like outlines, bright where moving
  shaders/mosh.glsl     feedback datamosh, ring layout, subject mask
  shaders/look.glsl     colour grade + audio-reactive motion glow
  shaders/clones.glsl   optional crowd of time-delayed copies
  scripts/              uniforms, wiring and plugin settings (see above)
  reference/            numpy mirror of the motion shader + tests
  vendor/               third-party plugin (gitignored)
  WALKTHROUGH.md        beginner build guide
capture/                camera and mask notes (Kinect plan, MediaPipe)
docs/decisions.md       decision log
```

## How it works

**Polyphony from YIN.** YIN's cumulative mean normalised difference
function (CMNDF) dips close to zero at the period of a single note. A chord
has no common period in the searched range, so its minimum stays high. On
synthetic plucked strings this separates notes from chords far better
than spectral flatness or peak counting, which both mistake a low E string
for a chord ([D1](docs/decisions.md)). Validation on real guitar takes is
still to do (`features/evaluate.py`).

**Reused DSP.** `features/yin.py` is a Python/numpy port of the YIN pitch
detector from my JUCE plugin *multivoicer*
(`multivoicer/Source/Dsp/YinPitchDetector.cpp`, branch
`feat/multivoicer-v1`). The algorithm steps are the same line for line.
The difference function uses an FFT autocorrelation instead of the double
loop, and a test checks that both agree.

**Two analysis windows.** Onsets use a 512-sample window for speed; bands
and YIN use 2048 samples, because a guitar's low E (82 Hz) has a period
longer than 512 samples ([D6](docs/decisions.md)).

**Datamosh as codec emulation.** A video codec predicts each frame by
moving blocks of the previous frame along motion vectors, and keyframes
periodically reset it. Datamoshing removes the keyframes, so stale
pixels get dragged along new motion. `motion.glsl` estimates one vector
per 8 × 8 block (Lucas-Kanade), as an encoder would ([D4](docs/decisions.md)).
`mosh.glsl` gives each block a fixed random threshold. While its ring's
audio amount is above that threshold, the block shows the previous output
warped by its motion vector instead of the live image, and stays that way
until the music drops. Blending every pixel by the amount instead made
trails fade within a few frames ([D9](docs/decisions.md)).

**Rings.** Band position is (distance from centre)^k; k = 3 gives the
lowest band about 22 % of the frame ([D10](docs/decisions.md)).

**Outlines.** `stylize.glsl` replaces the photo with thresholded Sobel
edges, brightened by local motion and coloured by the mask (performer
versus room). It runs before the mosh, so the mosh smears lines; the motion
pass still sees the raw camera ([D11](docs/decisions.md)).

**Clones.** Mask-cut copies of the finished image in pinhole perspective:
row r at depth z = 1 + r·d is scaled by 1/z towards a vanishing point and
fades with depth. Each row replays the performer a few frames later from
a 32-frame history (Texture 3D TOPs used as ring buffers). A lighten blend
keeps the performer's outlines on top ([D12](docs/decisions.md)).

## Measurements

On an Apple M1 MacBook Air, at 1280 × 720:

| What | Value | How |
|---|---|---|
| Output frame rate | **30 fps, steady** (locked to the camera rate) | Perform CHOP |
| Frame rate with the project left at 60 fps | 43 fps, uneven: every other frame repeated the camera image, so motion and mosh flickered | Perform CHOP; reason for locking to 30 |
| GPU/CPU cook time per shader pass | under 1 ms each (stylize 0.97 ms, mosh 0.18 ms, motion 0.18 ms) | operator cook times |
| Feature extraction | 0.35 ms per 5.3 ms hop (about 15× real-time headroom) | offline benchmark |
| Mic input latency (sounddevice) | 37.7 ms | reported by the audio stream |
| Audio-to-visual latency, end to end | to do | 240 fps phone video of a pluck and the screen |

## User test

To do (milestone 7).

## Credits

- MediaPipe TouchDesigner plugin by Torin Blankensmith
  ([torinmb/mediapipe-touchdesigner](https://github.com/torinmb/mediapipe-touchdesigner)),
  used for person segmentation.
- YIN: A. de Cheveigné and H. Kawahara, JASA 2002.
- Optical-flow colour coding: Baker et al., 2011.
