# moshviz

A real-time, audio-reactive datamosh for live performance, built in
TouchDesigner with hand-written GLSL and a Python audio-analysis service.

## What we are doing

A camera, or a recording of a show, is turned into a moving abstract image
that breaks apart with the music. Video blocks freeze and get dragged along
the movement, the way a video stream breaks when it loses its keyframes
(datamoshing).

- **Movement decides where.** The image breaks where things move: dancers,
  performers, light sweeps. The same moving blocks glow in colour, with hue
  showing the direction of movement.
- **Music decides how much.** Live audio analysis drives the strength:
  loud passages break the image harder, and silence keeps it intact. A
  frequency layout is also available, alone or mixed in: bass breaks a
  large disc at the centre, highs thin rings at the edges.
- **Melody vs chord → shape.** A polyphony measure (from YIN pitch
  detection) tells single notes from chords, so single notes break narrow
  areas and chords wide ones.
- **Not photographic.** The picture becomes lidar-like outlines plus the
  stage lights in their own colours, so the piece is about movement and
  light rather than the room.
- **Performer stays clean** (camera mode): a person mask keeps the
  performer intact while the world around them moshes.
- **Clones** (optional): a crowd of copies receding to a vanishing point,
  each row repeating the movement a few frames later.
- **Mirrored canvas:** the output is always 1280 × 720; any image that does
  not fill it (a portrait phone clip, for example) is continued with
  mirrored copies, with an optional symmetry mode for a kaleidoscope look.

**Why.** This is portfolio work for master's applications. Each part
shows a skill that the rest of the transcript does not:

| Part | Skill shown | For |
|---|---|---|
| Polyphony, onset and band features from live audio; YIN reused from my own plugin | Signal processing, music information retrieval | UPF Sound and Music Computing |
| Hand-written GLSL: motion estimation with illumination compensation, datamosh, stylisation, clones, mirror canvas | Real-time graphics programming | Utrecht Game and Media Technology |
| Live interactive audiovisual system with a control panel, plus a small user test | Interaction design and evaluation | KTH Interactive Media Technology |

The reasoning behind every significant choice, with evidence, is in the
[decision log](docs/decisions.md) (D1–D17).

## Pipeline

```
 audio (mic, guitar, or the clip's own sound)
   └─► features/service.py ──OSC──► oscin1 ─► rows_sel ─► rows (1 x 24 band amounts)
       bands, onsets, polyphony,                            │
       gate → 24 amounts                                    │
                                                            ▼
 webcam ─► MediaPipe plugin ─► mp_video ─┐          ┌──────────────────────────────┐
              └─► selfie mask ─► mask    ├─► prep ─► cam ─┬─► motion  (block motion vectors)
 show clip ─► movie ─► clip_xform ───────┘  (input  │      ├─► stylize (outlines + lights)
                                            grading)│      │       │
                                                    │      └──► mosh  (feedback datamosh:
                                                    │               motion and/or rings
                                                    │               decide where, audio how much)
                                                    │                │
                                                    │             look  (motion-colour glow)
                                                    │                │
                       motion ─► motion_mask ─► history buffers ─► clones (optional crowd)
                                                                     │
                                                                  frame (1280 x 720, mirror fill) ─► out
```

## Status

| # | Milestone | State |
|---|---|---|
| 1 | Camera | **Webcam + MediaPipe** in use; show recordings as a second source; Kinect deferred ([capture/README.md](capture/README.md)) |
| 2 | Subject mask | **Done**: MediaPipe selfie segmentation (camera); motion mask for clones on video |
| 3 | Feature service + tests | **Done**: 67 tests (features, service, motion maths, video converter) |
| 4 | Shaders | **Done**: prep, motion, stylize, mosh, look, clones, motion_mask, frame |
| 5 | Tuning | In progress: tuned with music through the mic and with show footage; guitar takes to do |
| 6 | Measurements | Partly done (see below); end-to-end audio-to-visual latency still to measure |
| 7 | Mini user test | To do |
| 8 | Demo video | To do |

## Running it

**1. Python feature service** (from the `moshviz` folder):

```bash
python3 -m venv .venv && .venv/bin/pip install -r features/requirements.txt
.venv/bin/pytest                                                       # 67 tests
.venv/bin/python features/service.py --list-devices
.venv/bin/python features/service.py --device "MacBook Air Microphone"   # live audio
.venv/bin/python features/service.py --device "<interface>" --channel 0  # guitar
.venv/bin/python features/service.py --file show.mov --play --loop --set threshold=0.7
```

`--play` plays a file's audio (sound or video file) and analyses exactly
what it plays, sending the playback position to TouchDesigner as the video
clock. `--set NAME=VALUE` starts with a mapping value (dense band audio
needs a higher threshold than a solo guitar). The service logs level,
polyphony, onset and strip amounts every 2 s.

**2. MediaPipe plugin.** Download `release.zip` of
[torinmb/mediapipe-touchdesigner](https://github.com/torinmb/mediapipe-touchdesigner)
v0.6.0 and unzip it to `td/vendor/mediapipe-touchdesigner/`. That folder is
gitignored (188 MB), and the `.toe` links to it as an external `.tox`.

**3. TouchDesigner.** Open `td/moshviz.toe`. Then, in the Textport
(Dialogs → Textport and DATs):

```python
exec(open(project.folder + '/scripts/mediapipe_settings.py').read())   # segmentation only
exec(open(project.folder + '/scripts/wiring.py').read())               # build/check/repair the network
exec(open(project.folder + '/scripts/uniforms.py').read())             # apply all shader values
```

**4. Live controls.** Click the **`controls`** node; its Controls page has
everything you adjust while it runs:

| Group | Controls |
|---|---|
| Effect | **Mosh from motion** (0 = frequency rings, 1 = movement decides where), **Clones** on/off |
| Input grading (before analysis) | **Exposure, Brightness, Contrast, Gamma, Saturation, Soften** (denoise) |
| Clip (movie mode) | **Clip start / end**, **Zoom**, **Offset X / Y**, **Rotate** |
| Output canvas | **Mirror fill**, **Mirror fade**, **Symmetry**, **Fit zoom** |

### Running on a show recording

```bash
.venv/bin/python tools/intra_frame.py recordings/show.mov   # upright, 30 fps, intra-frame copy
```

Set `MOVIE_FILE`, `MOVIE_SIZE` and `MOVIE_DURATION_S` in
`td/scripts/wiring.py` to the copy (the script prints its size and
length). Then choose the clock:

```python
# The clip's own sound drives the visuals (start the service with --file show.mov --play):
MOSHVIZ_SOURCE = 'movie'; MOSHVIZ_CLOCK = 'audio'; exec(open(project.folder + '/scripts/wiring.py').read())
# The clip loops silently; the visuals react to live audio (service on a mic or loopback device):
MOSHVIZ_SOURCE = 'movie'; MOSHVIZ_CLOCK = 'free'; exec(open(project.folder + '/scripts/wiring.py').read())
# Back to the webcam:
MOSHVIZ_SOURCE = 'camera'; exec(open(project.folder + '/scripts/wiring.py').read())
```

Re-run `uniforms.py` after switching: movie footage gets its own settings
(relative edges, light layer, smooth glow).

Step-by-step build guide: [td/WALKTHROUGH.md](td/WALKTHROUGH.md).
Compact reference and troubleshooting: [td/README.md](td/README.md).

## Everything lives in text

TouchDesigner saves projects as binary `.toe` files, which version control
cannot diff. So the `.toe` holds as little as possible:

- **Shaders** are `.glsl` files, loaded by Text DATs with Sync to File.
- **Network** (every wire, operator reference, created node and control
  parameter) lives in [td/scripts/wiring.py](td/scripts/wiring.py), which
  builds missing parts, repairs broken wiring, and switches between camera
  and movie.
- **Shader values** (every uniform, with per-source overrides and bindings
  to the control panel) live in [td/scripts/uniforms.py](td/scripts/uniforms.py).
- **Plugin settings** live in
  [td/scripts/mediapipe_settings.py](td/scripts/mediapipe_settings.py).
- **Audio mapping parameters** live in `features/mapping.py`, and can be
  changed live over OSC (`/moshviz/param/<name>` to port 9001).

## Layout

```
features/               Python feature service (audio in, OSC out)
  yin.py                YIN pitch detector, ported from multivoicer (see below)
  features.py           band energies, spectral-flux onsets, polyphony, smoothing
  mapping.py            features → 24 amounts (gate, threshold, sharpness, spread)
  service.py            live input or file playback (audio clock, trim), OSC
  evaluate.py           compares polyphony measures on labelled recordings
  synth.py              synthetic test signals (plucked strings, chords, clicks)
  tests/
td/
  moshviz.toe           TouchDesigner project (binary, minimal)
  shaders/prep.glsl         input grading before analysis
  shaders/motion.glsl       per-block Lucas-Kanade motion, illumination-compensated
  shaders/stylize.glsl      outlines (absolute or Weber-contrast edges) + light layer
  shaders/mosh.glsl         feedback datamosh; motion- or band-driven; subject mask
  shaders/look.glsl         colour grade + motion glow (flat blocks or smooth)
  shaders/motion_mask.glsl  "whatever moves" mask (clones on video)
  shaders/clones.glsl       optional crowd of time-delayed copies
  shaders/frame.glsl        fixed output canvas, mirror fill, symmetry
  scripts/              wiring, uniforms and plugin settings (see above)
  reference/            numpy mirror of the motion shader + tests
  vendor/               third-party plugin (gitignored)
  WALKTHROUGH.md        step-by-step build guide
tools/
  intra_frame.py        phone video → upright, constant-frame-rate ProRes for TouchDesigner
capture/                camera and mask notes (Kinect plan, MediaPipe)
docs/decisions.md       decision log
recordings/             local clips (gitignored)
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

**Motion estimation.** `motion.glsl` estimates one vector per macroblock
(Lucas-Kanade), as a video encoder would ([D4](docs/decisions.md)). Stage
lighting breaks LK's brightness-constancy assumption: a strobe reads as
motion everywhere. Each block therefore measures its brightness gain
between frames and compensates before differencing. In tests, a 60 %
flash drops from over 0.3 px of false motion to under 0.05 px, while real
movement during the flash is still recovered ([D14](docs/decisions.md)).

**Datamosh as codec emulation.** A codec predicts each frame by moving
blocks of the previous frame along motion vectors, and keyframes reset
it. Datamoshing removes the keyframes, so stale pixels get dragged along
new motion. `mosh.glsl` gives each block a fixed random threshold. While
the block's amount is above it, the block shows the previous output
warped by its motion vector, and stays that way until the amount drops
([D9](docs/decisions.md)). The amount comes from movement × audio energy
(motion drive, [D17](docs/decisions.md)), from the frequency rings
([D10](docs/decisions.md)), or a blend of the two.

**Outlines and light.** `stylize.glsl` replaces the photo with thresholded
Sobel edges, brightened by local motion. On dark stage footage it can
divide the gradient by local brightness (Weber contrast), so edges in
shadow count as much as edges under a spotlight. A light layer keeps only
the bright parts (spotlights, strobes, LEDs) in their own colour, with a
glow ([D11](docs/decisions.md), [D14](docs/decisions.md)).

**Input grading.** `prep.glsl` adjusts the input before any analysis, so
more contrast also means more motion and more edges to find
([D15](docs/decisions.md)).

**Clones.** Mask-cut copies of the finished image in pinhole perspective:
row r at depth z = 1 + r·d is scaled by 1/z towards a vanishing point and
fades with depth. Each row replays the movement a few frames later from a
32-frame history (Texture 3D TOPs used as ring buffers). On video, where
there is no person mask, a motion mask makes the clones copy whatever
moves ([D12](docs/decisions.md)).

**Mirror canvas.** `frame.glsl` fits the image inside a fixed 1280 × 720
canvas and folds coordinates outside it with a triangle wave,
1 − |mod(x, 2) − 1|, so mirrored copies join seamlessly
([D16](docs/decisions.md)).

**Video sync.** With a show recording, the audio is the master clock: the
service plays the sound and sends its position, and the movie shows the
frame at that time ([D13](docs/decisions.md)). Phone video has to be
converted first (`tools/intra_frame.py`): HEVC froze when seeked every
frame, and variable-frame-rate clips with a misleading nominal rate
(120 fps on a 29.5 fps clip) played four times too fast.

## Measurements

On an Apple M1 MacBook Air, at 1280 × 720 (the Non-Commercial licence cap):

| What | Value | How |
|---|---|---|
| Output frame rate | **30 fps, steady**, with the cook rate matched to the source | Perform CHOP |
| Frame rate with the project left at 60 fps | 43 fps, uneven: repeated camera frames made motion and mosh flicker | Perform CHOP; reason for matching the source rate |
| Cook time per shader pass | under 1 ms each (stylize 0.97 ms, mosh 0.18 ms, motion 0.18 ms) | operator cook times |
| Feature extraction | 0.35 ms per 5.3 ms hop (about 15× real-time headroom) | offline benchmark |
| Mic input latency | 37.7 ms | reported by the audio stream |
| Playback output latency (`--play`) | 8.4 ms | reported by the audio stream |
| Show footage brightness | 3 % mean; outline content 0.19 % → 5.1 % with relative edges and exposure lift | mean of TOP pixels |
| Audio-to-visual latency, end to end | to do | 240 fps phone video of a pluck and the screen |

## User test

To do (milestone 7).

## Credits

- MediaPipe TouchDesigner plugin by Torin Blankensmith
  ([torinmb/mediapipe-touchdesigner](https://github.com/torinmb/mediapipe-touchdesigner)),
  used for person segmentation.
- YIN: A. de Cheveigné and H. Kawahara, JASA 2002.
- Optical-flow colour coding: Baker et al., 2011.
