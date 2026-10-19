# TouchDesigner walkthrough (beginner, step by step)

This builds the moshviz network from an empty project. Every step ends
with a ✅ check. If a check fails, send Claude a screenshot or the error
text. [README.md](README.md) has the same network as a compact reference.

Terminal commands assume you are in the `moshviz` folder:

```bash
cd /Users/sohumdatta/repos/AudioDevProjects/.claude/worktrees/audio-reactive-datamosh-432670/moshviz
```

## TouchDesigner basics

- **Network editor:** the big grid where operators (nodes) go. Colours
  show the family: **TOP** = images (purple), **CHOP** = numbers and
  channels (green), **DAT** = text (pink).
- **Where you are:** the path bar at the top of the network editor. Build
  everything inside **`/project1`**. If you see only the `project1` and
  `perform` boxes, you are at the root: double-click `project1` (or select
  it and press **I**) to go in. **U** goes back up. Never delete `project1`
  or `perform`.
- **Add a node:** press **Tab** over empty grid space, pick the family tab
  (TOP / CHOP / DAT), click the operator, then click on the grid to place it.
- **Connect nodes:** drag from the connector on a node's **right edge** to
  the **left edge** of the next node.
- **Parameters:** click a node and its settings appear in the parameter
  panel (press **P** if it is hidden). The tabs across the top of the panel
  are pages (GLSL, Vectors, Common, ...). If the panel is narrow, the tab
  names are cut off (Common shows as `Comm`); click **»** or widen the panel.
- **Rename:** click the name label under a node and type. Use the names
  given here exactly.
- **Bigger preview:** right-click a node → **View…**
- **Info and errors:** select a node and click the **i** icon at the top of the
  parameter panel (or right-click → Info…). This shows the resolution and any
  error message. On a mouse, middle-click does the same.
- **Save often:** Cmd+S.

## Step 0: Project file ✔ done

The project is at `moshviz/td/moshviz.toe`. To find it again in a macOS
file dialog, press **Cmd+Shift+G** and paste this path (the `.claude`
folder is hidden because its name starts with a dot):

```
/Users/sohumdatta/repos/AudioDevProjects/.claude/worktrees/audio-reactive-datamosh-432670/moshviz/td
```

## Step 1: Camera ✔ done

1. Tab → TOP → **Video Device In**. If macOS asks, allow camera access.
2. Tab → TOP → **Null**. Connect Video Device In → Null, and rename the
   Null to **`cam`**.
3. Click the **i** icon for `cam` and note its **resolution**. Yours is **1280×720**.

✅ The `cam` thumbnail shows your camera image.

## Step 2: Previous frame ✔ done

1. Tab → TOP → **Feedback**. Connect `cam` → Feedback, and rename it
   **`prev_cam`**.
2. Set its **Target TOP** parameter to `cam` (type it, or drag the `cam`
   node onto the field).

✅ It looks the same as `cam` (it is one frame behind).

## Step 3: Motion pass (first shader) ✔ done

1. Tab → TOP → **GLSL**. Rename it **`motion`**. TouchDesigner also creates
   small DATs next to it: click the one whose name ends in **`_pixel`**.
2. In that DAT, set **File** to `shaders/motion.glsl` and turn **Sync to
   File** on. The DAT should now show the shader code.
3. Connect inputs in this order (hover over the input dots to see their
   numbers):
   - input 0: `cam` → `motion`
   - input 1: `prev_cam` → `motion`
4. On `motion`:
   - **Common** page: set **Output Resolution** to *Custom Resolution*
     and **Resolution** to **160 × 90** (your 1280×720 camera ÷ 8 px blocks). Set **Pixel Format** to **32-bit float (RGBA)**.
   - **GLSL** page: set **Input Extend Mode UV** to **Hold** (with *Zero*,
     the frame edges show fake motion). Click **Load** next to *Load Uniform
     Names*: this fills in the uniform names from the shader.
   - **Vectors** page: set the value of `uRegularisation` (first box) to `0.001`.

✅ No red X. Sitting still gives an almost black image. Waving a hand
left and right gives blocky **red** patches in one direction, and up and
down gives **green**.
❌ If there's a red X, click **i** on `motion` and send the error, or copy the text of the DAT ending in `_info` next to
`motion` and send it to Claude.

## Step 4: Audio → row texture ✔ done

1. Start the feature service in a terminal and leave it running (Ctrl+C
   stops it):
   ```bash
   .venv/bin/python features/service.py --file recordings/test_riff.wav --loop
   ```
2. Tab → CHOP → **OSC In**. Set **Network Port** to `9000`.
   ✅ Many channels bounce in time with the riff (shown as bars). They are
   named `moshviz/row/00` … `moshviz/row/23`, then `moshviz/onset`,
   `moshviz/polyphony`, `moshviz/pitch`, `moshviz/level`, `moshviz/seq`.
3. Tab → CHOP → **Select**. Connect OSC In → Select, and rename it
   **`rows_sel`**. Leave its **CHOPs** field **empty** (the wire supplies
   the input; pointing it at itself gives a ⚠). Set **Channel Names** to
   `moshviz/row/*`.
   ✅ Exactly 24 channels remain (click **i** to count).
4. Tab → TOP → **CHOP to TOP**. Rename it **`rows`**. Set **CHOP** to
   `rows_sel`, and **Data Format** to *R*. On the Common page, set **Pixel
   Format** to 32-bit float.
   ✅ Click **i**: the resolution is **1 × 24** (a vertical strip;
   confirmed). The preview is shades of **red** pulsing with the audio,
   because Data Format R uses only the red channel.

## Step 5: Stand-in subject mask ✔ done

Tab → TOP → **Circle**. Rename it **`mask_stub`**: a white disc on black.
It stands in for you until the Kinect works.

## Step 6: Mosh shader

1. Tab → TOP → **GLSL**. Rename it **`mosh`**. In its `_pixel` DAT, set
   **File** to `shaders/mosh.glsl`, with Sync to File on.
2. Tab → TOP → **Feedback**. Rename it **`mosh_fb`**. Connect `cam` →
   `mosh_fb`, and set its **Target TOP** to `mosh`.
3. Give `mosh` its five inputs. A GLSL TOP accepts only **three wired
   inputs**, so the last two go in a parameter instead:
   - **Wire** in this order (drag each onto `mosh`): 0 `cam`, 1 `mosh_fb`,
     2 `mask_stub`.
   - **GLSL** page → **TOPs** field: type `rows motion` and press Return.
     These become inputs 3 and 4 (confirmed with Check A).

   Until all five are present, `mosh` shows a red/blue checkerboard: the
   shader asks for inputs that do not exist yet. If a wire lands on the
   wrong input (dropping on a used connector replaces it), set all three
   from the Textport (**Dialogs → Textport and DATs**; paste with no
   leading space):
   ```python
   p = '/project1/'; op(p + 'mosh').setInputs([op(p + n) for n in ('cam', 'mosh_fb', 'mask_stub')])
   ```
   Check with `[i.name for i in op('/project1/mosh').inputs]`.
4. `mosh` **Common** page: set **Pixel Format** to **16-bit float (RGBA)**.
   Leave the resolution as it is. On the **GLSL** page, set **Input Extend
   Mode UV** to **Hold** and click **Load** (Load Uniform Names).
5. `mosh` **Vectors** page: the names should now be filled in. Set the values:

   | Uniform Name | value x |
   |---|---|
   | `uDisplacement` | 1 |
   | `uMaxMotionPx` | 16 |
   | `uRefresh` | 0.02 |
   | `uMaskFeather` | 0.1 |
   | `uThresholdSoftness` | 0.05 |
   | `uReseedFrames` | 0 |
   | `uRadial` | 1 |
   | `uRadialCurve` | 3 |
   | `uFrame` | `absTime.frame` (the field should show the frame number counting up) |

6. Tab → TOP → **Null**. Connect `mosh` → Null and rename it **`out`**.
   Right-click → View…

✅ **Check A (no audio):** make a **Ramp** TOP (vertical, resolution
1 × 24) and change the `mosh` **TOPs** field to `ramp1 motion`. When you
move, the **top** of the picture smears and the **bottom** stays normal.
The circle always stays clean. Then set TOPs back to `rows motion`.
(Passed.)

✅ **Check B (audio):** with the service running, single notes smear a few
thin bands and the chord smears large areas. Report whether low notes
affect the **bottom** bands (expected) or the top.

Save (Cmd+S).

## Step 6b: Look pass (colour grade + motion glow)

`mosh` output alpha carries the audio amount, so its own viewer may look
partly transparent. That is expected; `look` turns it back into a normal
image.

1. Tab → TOP → **GLSL**. Rename it **`look`**. In its `_pixel` DAT, set
   **File** to `shaders/look.glsl`, with Sync to File on.
2. Wire inputs: 0 `mosh`, 1 `motion`.
3. Rewire `out` so its input is `look` instead of `mosh` (drag from
   `look`'s output onto `out`; this replaces the old wire).
4. `look` **Common** page: **Pixel Format** 16-bit float (RGBA). **GLSL**
   page: click **Load** (Load Uniform Names).
5. **Vectors** page values (the tints use x, y, z as red, green, blue):

   | Uniform Name | value |
   |---|---|
   | `uBrightness` | -0.1 |
   | `uContrast` | 1.3 |
   | `uSaturation` | 0.6 |
   | `uDuotone` | 0 (try 0.7) |
   | `uShadowTint` | 0.05, 0.0, 0.15 |
   | `uHighlightTint` | 1.0, 0.75, 0.6 |
   | `uMotionGlow` | 0.3 |
   | `uGlowAudio` | 1.5 |
   | `uMotionScalePx` | 3 |
   | `uMotionFloorPx` | 0.3 |

✅ `out` shows a darker, punchier image. Moving makes coloured blocks glow
(colour = direction of movement), and they glow brighter while music plays.

## Step 7: Live guitar

```bash
.venv/bin/python features/service.py --list-devices
```

Find your interface and the guitar's input (inputs count from 0), then:

```bash
.venv/bin/python features/service.py --device "Scarlett" --channel 0
```

Replace `Scarlett` with part of your interface's name. If it reports a
sample-rate error, add `--sample-rate 44100`.

## Step 8: Subject mask with MediaPipe

The plugin lives in `td/vendor/mediapipe-touchdesigner/` (gitignored; see
`capture/README.md` for where to get it).

1. **Add the plugin** ✔ done. Drag `toxes/MediaPipe.tox` and
   `toxes/image_segmentation.tox` from Finder into `/project1`. On each
   node's **Common** page, **External .tox** should point at the file, and
   **Enable External .tox** should be **On**, so the `.toe` stays small
   (about 16 KB).
2. **Pick the webcam** ✔ done. Choose it from the camera dropdown on the
   MediaPipe node. It takes a few seconds the first time.
3. **Find the mask.** It is a Script TOP called `seg_data` somewhere
   inside the plugin. Paste this into the Textport:
   ```python
   [o.path for o in op('/project1').findChildren(name='seg_data')]
   ```
   ✔ It is `/project1/MediaPipe/seg_data`.
4. **Segmentation only.** Out of the box, every tracker except
   segmentation is on. Fix that from the Textport:
   ```python
   exec(open(project.folder + '/scripts/mediapipe_settings.py').read())
   ```
   This turns segmentation on (normal multiclass mode, which the
   `image_segmentation` component decodes), and turns face, hands, pose,
   objects, classification and embeddings off.
5. **Bring the mask out.** Tab → TOP → **Select**, renamed **`mask`**.
   Set its **TOP** parameter to `/project1/image_segmentation/selfie_mask`
   (white = you, full resolution, upright). `MediaPipe/seg_data` does not
   update in this setup.
   ✅ A white silhouette of you on black (soft, grey edges are fine).
6. **Use it.** In `mosh`, replace input 2 (`mask_stub`) with `mask`: drag
   from `mask`'s output onto the connector that `mask_stub` uses (dropping
   on a used connector replaces it). Or from the Textport:
   ```python
   p = '/project1/'; op(p + 'mosh').setInputs([op(p + n) for n in ('cam', 'mosh_fb', 'mask')])
   ```
   ✅ With music playing and you moving, the background moshes and your
   body stays clean. If edges look harsh, raise `uMaskFeather` in
   `scripts/uniforms.py`.
7. **Line up picture and mask.** Feed `cam` from the frame the mask was
   computed from, so they never drift apart: a Select TOP **`mp_video`**
   with **TOP** = `/project1/image_segmentation/original_video`, wired into
   `cam` in place of `video_in`.

## Step 9: Outline (lidar-style) view

Replaces the photographic image with glowing outlines. Moving edges are
bright, and still ones are dim. The datamosh then drags outlines instead
of the room.

1. Tab → TOP → **GLSL**, renamed **`stylize`**. In its `_pixel` DAT, set
   **File** to `shaders/stylize.glsl`, with Sync to File on.
2. Wire three inputs, in order: 0 `cam`, 1 `motion`, 2 `mask`.
3. **Common** page: **Pixel Format** 16-bit float (RGBA).
4. Feed `stylize` into `mosh` and `mosh_fb` in place of `cam` (the motion
   pass keeps using the real `cam`):
   ```python
   p = '/project1/'; op(p + 'mosh').setInputs([op(p + n) for n in ('stylize', 'mosh_fb', 'mask')]); op(p + 'mosh_fb').setInputs([op(p + 'stylize')])
   ```
5. Apply the values (this also adds the uniform rows):
   ```python
   exec(open(project.folder + '/scripts/uniforms.py').read())
   ```

✅ `out` is mostly dark, with outlines. Your body is outlined in warm
white and the room in cyan. Moving parts are bright and still parts dim.
Music makes the outlines smear and glow.

Knobs (in `scripts/uniforms.py`, `stylize` block): `uStaticLevel` 0 makes
still things vanish; `uPointMix` 1 gives dotted, scanner-like lines;
`uEdgeThreshold` higher removes noise sparkle; `uEdgeScalePx` higher
gives thicker lines; `uImageLevel` 0.1 brings back a faint photo.

## Step 10: Clones (optional, plug in / plug out)

A crowd of copies of you, in rows that recede to a vanishing point. Each
row further back is smaller, dimmer and a few frames later, so movement
ripples back through the crowd. Meant for a single performer; unplug it
for a real crowd.

1. Tab → TOP → **GLSL**, renamed **`clones`**. In its `_pixel` DAT, set
   **File** to `shaders/clones.glsl`, with Sync to File on. **Common** page:
   **Pixel Format** 16-bit float (RGBA).
2. Run the wiring script. It also **creates** `history_image` and
   `history_mask` (Texture 3D TOPs keeping the last 32 frames) and wires
   everything. Then set the values:
   ```python
   exec(open(project.folder + '/scripts/wiring.py').read())
   ```
   ```python
   exec(open(project.folder + '/scripts/uniforms.py').read())
   ```

✅ Rows of copies of you, getting smaller and dimmer towards the horizon,
each row moving a little later than the one in front.

**On / off:** click the **`controls`** node and flip **Clones** on its
Controls page. (The wiring script creates the panel and a `clones_switch`
that shows either `look` or `clones`.)
Knobs (`clones` block in `scripts/uniforms.py`): `uRows` (0 to 10),
`uColumns` (per side, 0 to 8), `uCloneSpacing`, `uRowDepth` (how fast rows
shrink), `uHorizon` (vanishing-point height), `uFog` (fade with distance),
`uRowDelayFrames` / `uColumnDelayFrames` (the ripple; 0 = in sync). The
total delay must stay under 32 frames (`HISTORY_FRAMES` in `wiring.py`).

## Step 11: Run it on a video file (e.g. a show recording)

The audio is the clock: the service plays the file's sound and analyses
exactly what it plays, and the movie shows the frame at that moment
(`/moshviz/time`), so sound and picture cannot drift. MediaPipe on macOS
only takes a webcam, so with a file there is **no performer mask**:
everyone moshes (the dance-floor case), and clones are bypassed.

1. Put the clip in `moshviz/recordings/` (gitignored) and make an
   intra-frame copy for TouchDesigner. Phone video (HEVC/H.264) freezes
   when the movie node seeks it every frame:
   ```bash
   .venv/bin/python tools/intra_frame.py recordings/IMG_1423.MOV
   ```
   For another clip, change `MOVIE_FILE` (the `_prores.mov` copy),
   `MOVIE_FPS` and `MOVIE_SIZE` in `scripts/wiring.py`. The service plays
   the original file's audio.
2. Terminal (stop any other running service first: only one can use the
   OSC ports):
   ```bash
   .venv/bin/python features/service.py --file recordings/IMG_1423.MOV --play --loop --set threshold=0.7
   ```
3. Textport: switch the network to the movie. This creates `movie`
   (Movie File In) and `no_mask` (black), rewires `cam`, bypasses
   `clones`, sets the cook rate to the clip's frame rate and keeps 8 px motion blocks:
   ```python
   MOSHVIZ_SOURCE = 'movie'; exec(open(project.folder + '/scripts/wiring.py').read())
   ```

✅ You hear the show, and `out` shows it as outlines that mosh with the
music: bass in the centre, highs at the edges.

4. Apply the values. Movie mode lifts exposure for dark stage footage:
   ```python
   exec(open(project.folder + '/scripts/uniforms.py').read())
   ```

**Control panel:** click the **`controls`** node. Its **Controls** page
has everything in one place:

| Control | What it does |
|---|---|
| **Clones** | on/off switch for the clones. With the camera they copy you (MediaPipe mask); with a movie they copy whatever moves (`motion_mask`) |
| **Clip start (s)**, **Clip end (s, 0 = end)** | the part of the clip that loops (with the audio clock, picture and sound together) |
| **Zoom** | 1 = original size, 1.5 = zoomed in |
| **Offset X**, **Offset Y** | moves the footage, as a fraction of the frame (0.1 = 10 %) |
| **Rotate (deg)** | rotates the footage |
| **Input exposure / brightness / contrast / gamma / saturation** | grade the input *before* motion and outline analysis (the `prep` pass): more contrast gives the analysis more to find |
| **Input soften (denoise)** | 0 to 1; blurs sensor noise away so dark grain is not mistaken for motion or edges |
| **Mosh from motion** | 0 = the music's frequency rings decide where the image breaks; 1 = movement decides where (the colour-glowing blocks break most) and the music decides how much |
| **Mirror fill** | fills the fixed 1280 × 720 canvas around the image with mirrored copies (off = black bars) |
| **Mirror fade** | brightness of each copy further out (1 = all equal) |
| **Symmetry** | mirrors the left half onto the right: a kaleidoscope look |
| **Fit zoom** | 1 = whole image fits the canvas; above 1 zooms in, mirrors continue it |

`clip_timing`, `clip_xform` and `clones_switch` read these through
expressions, so you never need to edit those nodes directly.

**Block size:** `MOVIE_BLOCK_PX` (4) and `CAMERA_BLOCK_PX` (8) in
`scripts/wiring.py`; re-run the wiring line after changing them.

**Back to the camera:**
```python
MOSHVIZ_SOURCE = 'camera'; exec(open(project.folder + '/scripts/wiring.py').read()); exec(open(project.folder + '/scripts/uniforms.py').read())
```

## If the network misbehaves

`scripts/wiring.py` holds every connection in the network. It reports any
wire or reference that differs from the file, and fixes it:

```python
exec(open(project.folder + '/scripts/wiring.py').read())
```

Example: a wire accidentally dropped onto `cam` from `motion` creates a
cook loop (⚠ "Cook dependency loop detected"), and everything turns black.

## Send back to Claude

1. Which ✅ checks passed, plus screenshots of the network and of `out`
   during a chord.
2. The OSC channel names (step 4.2) and the `rows` resolution (step 4.4).
3. Any `_info` error text.
