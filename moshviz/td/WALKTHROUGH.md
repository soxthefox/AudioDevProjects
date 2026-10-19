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
  are pages (GLSL, Vectors, Common, ...).
- **Rename:** click the name label under a node and type. Use the names
  given here exactly.
- **Bigger preview:** right-click a node → **View…**
- **Errors:** a red X on a node. Middle-click the node to read the error.
- **Save often:** Cmd+S.

## Step 0: Project file ✔ done

The project is at `moshviz/td/moshviz.toe`. To find it again in a macOS
file dialog, press **Cmd+Shift+G** and paste this path (the `.claude`
folder is hidden because its name starts with a dot):

```
/Users/sohumdatta/repos/AudioDevProjects/.claude/worktrees/audio-reactive-datamosh-432670/moshviz/td
```

## Step 1: Camera

1. Tab → TOP → **Video Device In**. If macOS asks, allow camera access.
2. Tab → TOP → **Null**. Connect Video Device In → Null, and rename the
   Null to **`cam`**.
3. Middle-click `cam` and note its **resolution** (for example 1280x720).

✅ The `cam` thumbnail shows your camera image.

## Step 2: Previous frame

1. Tab → TOP → **Feedback**. Connect `cam` → Feedback, and rename it
   **`prev_cam`**.
2. Set its **Target TOP** parameter to `cam` (type it, or drag the `cam`
   node onto the field).

✅ It looks the same as `cam` (it is one frame behind).

## Step 3: Motion pass (first shader)

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
     and **Resolution** to camera size ÷ 8 (1280×720 → **160 × 90**;
     640×480 → 80 × 60). Set **Pixel Format** to **32-bit float (RGBA)**.
   - **Vectors** page: set **Uniform Name 0** to `uRegularisation`, value
     x = `0.001`.

✅ No red X. Sitting still gives an almost black image. Waving a hand
left and right gives blocky **red** patches in one direction, and up and
down gives **green**.
❌ If there's a red X, copy the text of the DAT ending in `_info` next to
`motion` and send it to Claude.

## Step 4: Audio → row texture

1. Start the feature service in a terminal and leave it running (Ctrl+C
   stops it):
   ```bash
   .venv/bin/python features/service.py --file recordings/test_riff.wav --loop
   ```
2. Tab → CHOP → **OSC In**. Set **Network Port** to `9000`.
   ✅ Many channels bounce in time with the riff. Note their names
   (expected: something like `moshviz/row/00`).
3. Tab → CHOP → **Select**. Connect OSC In → Select, and rename it
   **`rows_sel`**. Set **Channel Names** to `*row*`.
   ✅ Exactly 24 channels remain (0 means the names differ: tell Claude).
4. Tab → TOP → **CHOP to TOP**. Rename it **`rows`**. Set **CHOP** to
   `rows_sel`, and **Data Format** to *R*. On the Common page, set **Pixel
   Format** to 32-bit float.
   ✅ Middle-click: the resolution is **1 × 24** (report it if it shows
   24 × 1). The preview flickers with the audio.

## Step 5: Stand-in subject mask

Tab → TOP → **Circle**. Rename it **`mask_stub`**: a white disc on black.
It stands in for you until the Kinect works.

## Step 6: Mosh shader

1. Tab → TOP → **GLSL**. Rename it **`mosh`**. In its `_pixel` DAT, set
   **File** to `shaders/mosh.glsl`, with Sync to File on.
2. Tab → TOP → **Feedback**. Rename it **`mosh_fb`**. Connect `cam` →
   `mosh_fb`, and set its **Target TOP** to `mosh`.
3. Connect into `mosh` in exactly this order:
   0 `cam`, 1 `mosh_fb`, 2 `mask_stub`, 3 `rows`, 4 `motion`.
4. `mosh` **Common** page: set **Pixel Format** to **16-bit float (RGBA)**.
   Leave the resolution as it is.
5. `mosh` **Vectors** page: use **+** to add six rows:

   | Uniform Name | value x |
   |---|---|
   | `uDisplacement` | 1 |
   | `uMaxMotionPx` | 16 |
   | `uRefresh` | 0.02 |
   | `uMaskFeather` | 0.1 |
   | `uBlockDropout` | 0 |
   | `uFrame` | `absTime.frame` (the field should show the frame number counting up) |

6. Tab → TOP → **Null**. Connect `mosh` → Null and rename it **`out`**.
   Right-click → View…

✅ **Check A (no audio):** make a **Ramp** TOP (vertical, resolution
1 × 24) and connect it into `mosh` input 3 in place of `rows`. When you
move, the **top** of the picture smears and the **bottom** stays normal.
The circle always stays clean. Then put `rows` back.

✅ **Check B (audio):** with the service running, single notes smear a few
thin bands and the chord smears large areas. Report whether low notes
affect the **bottom** bands (expected) or the top.

Save (Cmd+S).

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

## Send back to Claude

1. Which ✅ checks passed, plus screenshots of the network and of `out`
   during a chord.
2. The OSC channel names (step 4.2) and the `rows` resolution (step 4.4).
3. Any `_info` error text.
