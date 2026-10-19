# TouchDesigner network (reference)

Beginner build guide: [WALKTHROUGH.md](WALKTHROUGH.md). This page is the
compact reference for the current network.

All logic lives in text files; the `.toe` only holds operators. The source
of truth for each part:

| What | File |
|---|---|
| Shader code | `shaders/*.glsl`, loaded by each GLSL TOP's `_pixel` Text DAT (File + Sync to File) |
| Every wire and operator reference | `scripts/wiring.py` (reports and repairs differences; creates the history nodes) |
| Every shader uniform value | `scripts/uniforms.py` (adds missing uniform rows) |
| MediaPipe plugin settings | `scripts/mediapipe_settings.py` |

Run each from the Textport with
`exec(open(project.folder + '/scripts/<name>.py').read())`. Paths are
relative to the `.toe`, so the repo works from any clone location.

## Network

Created or checked by `scripts/wiring.py` (camera mode shown first; movie
mode swaps the source and the clone mask):

```
MediaPipe (plugin, webcam) ─► image_segmentation ─┬─ original_video ─► mp_video (Select) ─┐
                                                  └─ selfie_mask ────► mask (Select)       │
movie (Movie File In) ─► clip_xform (Transform) ───────────────────────── [movie mode] ────┤
                                                                                           ▼
                                                           prep (GLSL, input grading) ─► cam (Null)
cam ─► prev_cam (Feedback, target cam)
cam, prev_cam ─────────────────► motion       (GLSL, source size / block px, 32-bit float)
motion ────────────────────────► motion_mask  (GLSL, "whatever moves", clones on video)
cam, motion, mask* ────────────► stylize      (GLSL: outlines + light layer)
stylize ─► mosh_fb (Feedback, target mosh)
stylize, mosh_fb, mask* ───────► mosh         (GLSL; TOPs: rows motion)
oscin1 (OSC In, 9000) ─► rows_sel (Select CHOP, *row*) ─► rows (CHOP to TOP, 1 x 24, R, 32-bit)
mosh, motion ──────────────────► look         (GLSL: grade + motion glow)
look ─► history_image (Texture 3D, 2D array, 32 frames)
clone mask ─► history_mask (Texture 3D, 2D array, 32 frames)
look, history_image, history_mask ─► clones   (GLSL; TOPs: clone mask)
look, clones ──────────────────► clones_switch (Switch; index = Clones control)
clones_switch ─► frame (GLSL, 1280 x 720, mirror fill) ─► out (Null)
controls (Base COMP)   control panel: custom parameters, read by expressions
clip_timing ─► clip_osc (OSC Out, 9001)   clip trim to the service
perf (Perform CHOP), movie_info (Info CHOP)   readouts
```

`mask*` is `mask` with the camera and `no_mask` (black) with a movie. The
clone mask is `mask` with the camera and `motion_mask` with a movie.

Settings that matter:

- **Cook rate matches the source** (30 fps camera; the movie's rate). The
  wiring script sets it. A mismatch repeats frames, and the motion field
  flickers on the repeats.
- **GLSL TOPs:** 16-bit float RGBA, except `motion` (32-bit float, custom
  resolution = source size ÷ block size). Input Extend Mode UV: Hold.
- **A GLSL TOP takes at most three wired inputs.** Extra inputs go in its
  **TOPs** parameter and are numbered after the wired ones. Inputs are also
  grouped by texture type: 2D texture arrays (the history nodes) appear as
  `sTD2DArrayInputs[]`, separately from `sTD2DInputs[]`.
- `mosh` writes each block's audio amount into its output alpha for
  `look`; `look`, `clones` and `frame` output alpha 1.
- **Movie files** must be intra-frame and constant frame rate
  (`tools/intra_frame.py`). If you replace a converted file, give it a new
  name: TouchDesigner caches a movie's metadata by path.

## Live audio tuning

Send OSC to `127.0.0.1:9001` at `/moshviz/param/<name>`. The names are the
fields of `MappingParams` in `features/mapping.py`: `gate_db`,
`threshold`, `gain`, `melody_sharpness`, `max_spread_strips`,
`onset_boost`, and so on.

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| Red/blue checkerboard | Shader compile error. Read the `_info` DAT of the first node that shows it. Often a missing input |
| Everything black, ⚠ "Cook dependency loop" | A stray wire. Run `scripts/wiring.py` |
| Shader edit not showing up | Sync to File did not reload. Toggle it off and on |
| Stuck black/frozen after errors | Pulse **Reset** on `mosh_fb` |
| New uniforms missing | Run `scripts/uniforms.py` (it adds rows). The GLSL page's Load button does not rename or remove existing rows |
| Mask frozen | MediaPipe "background only" mode breaks `image_segmentation`; run `scripts/mediapipe_settings.py` |
| Movie freezes after a few seconds | It is HEVC/H.264; convert it with `tools/intra_frame.py` |
| Movie plays too fast | Variable-frame-rate phone clip, or a cached old file: convert it, and load the copy under a new name |
| Custom slider stops at 1 | Default slider range; run `scripts/wiring.py` (sets ranges) or type the value |
| Preview shows dots in 3D | The viewer is in point mode: activate the viewer (A), right-click inside it → View as Image (V) |
