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

```
MediaPipe (plugin, webcam) ─► image_segmentation ─┬─ original_video ─► mp_video (Select) ─► cam (Null)
                                                  └─ selfie_mask ────► mask (Select)
cam ─► prev_cam (Feedback, target cam)
cam, prev_cam ─────────────────► motion   (GLSL, 160 x 90, 32-bit float)
cam, motion, mask ─────────────► stylize  (GLSL)
stylize ─► mosh_fb (Feedback, target mosh)
stylize, mosh_fb, mask ────────► mosh     (GLSL; TOPs: rows motion)
oscin1 (OSC In, port 9000) ─► rows_sel (Select CHOP, *row*) ─► rows (CHOP to TOP, 1 x 24, R, 32-bit)
mosh, motion ──────────────────► look     (GLSL)
look ─► history_image (Texture 3D, 2D array, 32 frames)
mask ─► history_mask  (Texture 3D, 2D array, 32 frames)
look, history_image, history_mask ─► clones (GLSL; TOPs: mask; bypass to unplug) ─► out (Null)
perf (Perform CHOP)  frame-rate readout
```

Settings that are not in the scripts:

- **Project cook rate: 30 fps**, matching the camera. At 60 fps, every
  other frame repeats the camera image and the motion field flickers.
- **GLSL TOPs:** Pixel Format 16-bit float (RGBA) for `stylize`, `mosh`,
  `look` and `clones`; 32-bit float and a custom 160 × 90 resolution for
  `motion` (1280 × 720 ÷ 8 px blocks). Input Extend Mode UV: Hold.
- **A GLSL TOP takes at most three wired inputs.** Extra inputs go in its
  **TOPs** parameter and are numbered after the wired ones. Inputs are
  also grouped by texture type: 2D texture arrays (the history nodes)
  appear as `sTD2DArrayInputs[]`, separately from `sTD2DInputs[]`.
- `mosh` writes each block's audio amount into its output alpha for
  `look`; `look` and `clones` output alpha 1.

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
