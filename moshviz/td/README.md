# TouchDesigner network

All logic lives in text files. The `.toe` only wires operators together and
loads the shaders from disk. To load a shader: Text DAT, set **File** to the
path, and turn **Sync to File** on. After that, saving the `.glsl` in an
editor recompiles it live. Compile errors appear in the GLSL TOP's info
(middle-click the node, or attach an Info DAT).

Save the project as `td/moshviz.toe`. Use paths relative to the `.toe`
(for example `shaders/mosh.glsl`) so the repo works from any clone location.

## Network (v1, before the Kinect)

Until the camera spike is done, use the Mac's built-in camera, with a
Circle TOP standing in for the subject mask.

```
videodevicein1 ─► cam (Null) ─┬──────────────────────────────► mosh (GLSL, wired input 0)
                              ├─► prev_cam (Feedback, Target TOP = cam)
                              │        │
                              ├────────┴─► motion (GLSL) ────► mosh input 4 (TOPs parameter)
                              └─► mosh_fb (Feedback, Target TOP = mosh) ─► mosh wired input 1
mask_stub (Circle TOP, white disc on black) ───────────────────► mosh wired input 2
oscin1 (OSC In CHOP, port 9000) ─► rows_sel (Select CHOP) ─► rows (CHOP to TOP) ─► mosh input 3 (TOPs parameter)
mosh ─► out (Null) ─► moviefileout1 (for the demo, later)
```

| Operator | Settings |
|---|---|
| `motion_pixel`, `mosh_pixel` (Text DAT) | File: `shaders/motion.glsl`, `shaders/mosh.glsl`; Sync to File: on |
| `motion` (GLSL TOP) | Pixel Shader: `motion_pixel`. Common page: Output Resolution Custom, **80 × 60** (camera size ÷ 8 px blocks); Pixel Format **32-bit float (RGBA)**. Vectors: `uRegularisation` = 0.001 |
| `prev_cam` (Feedback TOP) | Input: `cam`. Target TOP: `cam` |
| `mosh_fb` (Feedback TOP) | Input: `cam`. Target TOP: `mosh` |
| `oscin1` (OSC In CHOP) | Network Port 9000 |
| `rows_sel` (Select CHOP) | CHOPs: empty (fed by the wire). Channel Names: `moshviz/row/*`, so only the 24 strip channels pass through |
| `rows` (CHOP to TOP) | Data Format: R. Pixel Format: 32-bit float. This gives a 1 × 24 texture, one row per channel |
| `mosh` (GLSL TOP) | Wired inputs: `cam`, `mosh_fb`, `mask_stub` (a GLSL TOP takes at most 3 wired inputs). **TOPs**: `rows motion`, which become inputs 3 and 4. Input Extend Mode UV: Hold. Pixel Shader: `mosh_pixel`. Output Resolution: same as `cam`. Pixel Format: 16-bit float (RGBA) or higher, because repeated feedback in 8-bit loses colour detail |

`mosh` uniforms (Vectors page; name in the left field, value in x):

| Uniform | Start value | What it does |
|---|---|---|
| `uDisplacement` | 1.0 | Gain on the motion vectors |
| `uMaxMotionPx` | 16 | Per-frame displacement clamp |
| `uRefresh` | 0.02 | How quickly moshed pixels heal toward the live image |
| `uMaskFeather` | 0.1 | Softness of the subject edge |
| `uBlockDropout` | 0.0 | 0 = smooth blend, 1 = blocks drop in and out |
| `uFrame` | `absTime.frame` (expression) | Reseeds the dropout every frame |

To tune the audio mapping live, send OSC from TouchDesigner (OSC Out CHOP
or DAT) to `127.0.0.1:9001` at `/moshviz/param/<name>`. The names are the
fields of `MappingParams` in `features/mapping.py`, such as `gain`,
`chord_gain` and `max_spread_strips`.

## What you should see

Things I could not verify from here are marked **(confirm)**. Please tell
me what you actually see.

1. **Motion pass.** View `motion` while you move a hand left and right in
   front of the camera. Blocks around the hand should turn red as it moves
   one way and go black the other way: the viewer clips negative values,
   and red is horizontal motion. Up and down movement shows in green. A
   still scene should be almost all black. **(confirm the direction: if
   moving right gives black, the camera image is mirrored, which is fine)**
2. **Strip mapping, without audio.** Temporarily set the `mosh` TOPs
   field to `ramp1 motion`, using a **Ramp TOP** (vertical, black at the bottom, white at the
   top, resolution 1 × 24). The top of the frame should smear and drag
   trails when you move, and the bottom should stay live. With a
   **Constant TOP** (white) in place of the ramp, the whole frame outside the circle
   moshes, and the circle region always stays live.
3. **OSC rows.** Start the service (see the main README). Use `--file` on
   any guitar recording if no interface is connected. `oscin1` should
   show channels `moshviz/row/00` … `moshviz/row/23` (confirmed). `rows`
   shows a thin column that flickers with the audio.
4. **Orientation.** Play a low note: the bottom strips should react. If the
   top strips react instead, CHOP to TOP is ordering the channels the other
   way. **(confirm)** Tell me and I'll flip `stripAmount()`.
5. **End to end.** Single notes should mosh a few thin bands, and a strummed
   chord should mosh wide areas. The circle always stays clean.
