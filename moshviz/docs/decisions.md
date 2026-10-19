# Decision log

Each entry records what was decided, the alternatives, why, and the
evidence. Status: **decided**, **provisional** (decided on synthetic or
indirect evidence; revisit with real data), or **open**.

---

## D1. Polyphony measure: YIN CMNDF minimum (provisional)

**Question.** Which audio feature tells a single melodic note from a
strummed chord, independent of loudness?

**Alternatives.** (a) YIN confidence: the global minimum of the cumulative
mean normalised difference function (CMNDF). (b) Spectral flatness. (c)
Count of significant spectral peaks.

**Decision.** (a), mapped linearly to 0..1 between CMNDF minima of 0.05
and 0.30.

**Why.** A single note is periodic, so the CMNDF dips close to 0 at its
period. An equal-tempered chord has no common period within the 60 Hz to
2 kHz lag range, so the minimum stays high. Flatness and peak count both
measure how *busy* the spectrum is, and a low string with many harmonics,
or a noisy pick attack, is busy too.

**Evidence (synthetic, 2048-sample frames at 48 kHz; `tests/test_features.py`, `evaluate.py`):**

| Signal | CMNDF min (median) | Flatness | Peak count |
|---|---|---|---|
| Sines, harmonic tones, plucked G3/E4 | 0.000 | ≤ 0.001 | 1 to 16 |
| Plucked low E (82 Hz) | 0.004 | **0.139** | **56** |
| C major triad, plucked | 0.175 | 0.039 | 42 |
| Open E major, plucked | 0.341 | 0.126 | 52 |
| Open G major, plucked | 0.471 | 0.092 | 48 |

AUC (chord frame scores above single-note frame), synthetic takes:
CMNDF 0.98, flatness 0.70, peak count 0.64. The plucked low E is the
critical case. It scores like a chord on flatness and peak count, but it
is clearly single on CMNDF.

**To do.** Record real takes (`recordings/single_*.wav`,
`recordings/chord_*.wav`; include low-string riffs, double stops and
power chords), then run `python features/evaluate.py recordings/`. Paste
the table here and re-fit the floor and ceiling.

**Known risks.**
- Pick attacks are noisy, so polyphony spikes briefly at every pluck. In the
  synthetic riff, raw polyphony reached 0.61 during a single-note attack;
  the 30 ms attack smoothing absorbs part of that. If real takes show the
  same, hold polyphony for about 30 ms after each onset.
- Power chords (root, fifth, octave) are nearly harmonic. The synthetic E5
  still scored 0.35, because its common period (2 × 12 ms) is outside the
  lag range, but real power chords on a guitar may score lower.

---

## D2. Camera path: webcam + MediaPipe for now, Kinect deferred (provisional)

Alternatives: Kinect v1 through libfreenect (depth mask, needs Homebrew
drivers and a bridge into TouchDesigner on Apple Silicon), or a webcam
with MediaPipe segmentation.

**Decision (19 Oct 2026).** Webcam + the MediaPipe TouchDesigner plugin,
chosen so the rest of the piece could be built and tuned without a
hardware spike. It runs segmentation on the GPU inside TouchDesigner and
provides the mask *and* the exact frame it was computed from
(`original_video`), so picture and mask cannot drift apart. The shaders
only see "a white-is-subject texture", so a Kinect depth mask can replace
it later without other changes.

**Observed.** The plugin enables every model by default (face, hands,
pose, objects, classification, embeddings); only segmentation is used,
so `scripts/mediapipe_settings.py` turns the rest off. Its
"background only" mode outputs a format that its own
`image_segmentation` decoder cannot read (outputs freeze), so the normal
multiclass mode is kept and `selfie_mask` is used. Segmentation sometimes
misses fast-moving hands; the clones pass's lighten blend (D12) hides
that.

**Open.** Kinect depth would give cleaner edges and work in the dark, and
it is the original plan (`capture/README.md`).

## D3. Transport into TouchDesigner (open; not needed while MediaPipe runs inside TouchDesigner)

Syphon, NDI, or raw frames into a Script TOP. To be decided by the spike,
comparing measured latency and frame rate.

---

## D4. Motion vectors: per-block Lucas-Kanade in a separate pass (decided)

**Alternatives.** (a) Plain frame differencing (|current − previous| as a
smear mask, no direction). (b) Exhaustive block matching (SAD over a search
window, as in a codec encoder). (c) Gradient-based Lucas-Kanade, solved
once per macroblock.

**Decision.** (c), in its own GLSL pass rendered at block resolution
(80 × 60 for 8 px blocks).

**Why.**
- (a) gives *where* things changed but not *which way*. Datamoshing needs a
  direction to drag stale pixels along.
- (b) is what codecs actually do. In a fragment shader it costs search
  window × block pixels taps per block: 9 × 9 offsets × 64 pixels gives
  5184 taps, against 80 for LK with 16 samples. It also only produces
  whole-pixel vectors, unless refined further.
- (c) gives sub-pixel vectors from one small linear solve per block.
  Keeping one vector per block reproduces codec macroblocks, which is
  what makes moshing look like moshing. The separate pass makes the
  motion field viewable for debugging, and it is computed once per block
  rather than once per pixel.
- Ridge regularisation keeps flat or textureless blocks at zero motion
  rather than noise.

**Limitation.** No image pyramid, so motion faster than about one gradient
spacing per frame (2 px at 8 px blocks) is underestimated. That is
acceptable because the output gets exaggerated with `uDisplacement`
anyway. If the smear looks too timid on fast strums, add a coarse-to-fine
level.

**Evidence.** `td/reference/motion_reference.py` mirrors the shader in
numpy. `test_motion_reference.py` recovers known translations, including
sub-pixel and diagonal ones, within 0.15 px, which also confirms the sign
convention used by the mosh pass.

---

## D5. Feature computation: standalone Python service (decided)

**Alternatives.** TouchDesigner CHOPs (Audio Spectrum, Analyze), or a
Script CHOP inside TouchDesigner.

**Decision.** Standalone Python service (numpy + sounddevice), sending OSC.

**Why.**
- Unit-testable without TouchDesigner: 47 feature tests run headless in
  about 2 s.
- It can reuse Sohum's own YIN (ported from the multivoicer C++ plugin).
  TouchDesigner has no CMNDF.
- Its process is isolated from TouchDesigner's frame loop, so a slow render
  frame cannot stall audio analysis.
- Cost: 0.35 ms of compute per 5.3 ms hop (measured on the M1, 5 s of
  plucked chord), so about 15× real-time headroom.

**Trade-off.** OSC adds a network hop. On localhost it is expected to be
sub-millisecond; it will be covered by the end-to-end latency measurement
(milestone 6).

---

## D6. Two analysis windows instead of one 512-sample window (decided)

The handover proposed 512-sample frames (about 10.7 ms). That is too short
for two of the three features:
- YIN needs at least two periods in the frame. The low E string's period
  is 582 samples at 48 kHz, longer than the whole 512-sample frame.
- Band energies at 512 points have a 94 Hz bin spacing, so all the
  log-spaced bands below about 200 Hz would share one or two bins.

**Decision.** Same 256-sample hop (5.3 ms update rate) for everything, but
onset uses a 512-sample window and bands and YIN use a 2048-sample window
(42.7 ms, the multivoicer frame size). Onsets, which drive the
"something just happened" kick, stay fast. Bands and polyphony are
smoothed with 10 to 30 ms attacks anyway, so the longer window costs
little perceived latency. Bands narrower than two FFT bins are widened
around their centre, so no strip is permanently dead.

---

## D7. Strip mapping: sharpness and spread driven by polyphony (provisional)

Band energy times a gain alone makes melodies and chords look alike, just
louder or quieter. Two polyphony-driven shapes make the difference visible:
- **Sharpness:** band levels are raised to a power, 4 for a single note
  down to 1 for a chord. Only a note's strongest partials survive, which
  gives thin strips.
- **Spread:** a max-preserving Gaussian dilation across strips, with
  sigma = 3 strips × polyphony. A chord's lit bands merge into large areas.

All constants are live OSC parameters (`MappingParams`). The final values
will come from tuning with the guitar (milestone 5).

---

## D9. Mosh weighting: persistent per-block selection, not per-pixel blending (decided)

**First version.** Each frame, output = mix(live, stale, amount). This
looked almost unmoshed on the first live test: with amount 0.5, half the
trail is replaced by the live frame every frame, so a smear decays as
0.5^n and is gone within 3 to 4 frames (about 0.1 s). That held even
though the row amounts from the mic were 0.4 to 0.8 (measured offline
from 4 s of mic input).

**Decision.** Each macroblock gets a fixed random threshold. A block
shows the stale, motion-warped image *fully* while its strip's amount is
above its threshold, and heals only through the small `uRefresh` term.
The amount therefore sets the *fraction of blocks* in a strip that mosh,
and those blocks stay moshed while the music holds. This is closer to a
codec, where a macroblock is either predicted from a stale reference or
refreshed. A block snapping back when the amount drops is the visual
equivalent of a keyframe arriving.

**Parameters.** `uThresholdSoftness` (a hard or soft switch per block)
and `uReseedFrames` (how often the set of moshing blocks reshuffles)
replace `uBlockDropout`.

---

## D10. Band layout: concentric rings, bass at the centre (provisional)

**Alternatives.** (a) Horizontal strips, with the lowest band at the
bottom (the original handover idea). (b) Concentric rings around the frame
centre, with the lowest band in the middle.

**Decision.** (b) by default, with a live toggle (`uRadial`) back to (a).
Sohum's call after the first live test: bass reads as the "core" of the
music and the highs as detail at the periphery, and rings make that
visible.

**Ring spacing.** Band position = r^k, where r is the aspect-corrected
distance from the centre (1 at the corners). Ring i then covers radii
(i/N)^(1/k) to ((i+1)/N)^(1/k). k = 2 gives equal-area rings; k > 2
enlarges the inner rings. Simulated share of a 1280 × 720 frame per band
(24 bands):

| k | lowest | 2nd | 3rd | highest 4 |
|---|---|---|---|---|
| 2 | 8 % | 8 % | 8 % | 0 to 2 % |
| 3 | 22 % | 13 % | 11 % | 0 to 1 % |
| 4 | 38 % | 14 % | 8 % | 0 % |

Default k = 3. The top bands only reach the corners, which suits guitar
and most music, since there is little energy above 3 kHz. The amount is
read at each macroblock's centre, so a block is never split between two
bands.

---

## D11. Visual style: outlines before the mosh, motion analysis on the raw camera (decided)

Sohum wanted the piece to be about movement, not to look like his room.
A stylize pass turns the camera image into thresholded Sobel outlines.
Moving edges are bright and static edges dim (`uStaticLevel`), and the
outlines can be broken into a dot grid for a scanner-like look. Outlines
are coloured by the subject mask, which separates the performer from the
room like depth would.

**Order.** Stylize runs *before* mosh, so the datamosh smears outlines,
which reads as line trails. The motion pass still runs on the raw camera:
outline images are mostly black, with few gradients, and would make
Lucas-Kanade motion estimates far less reliable.

---

## D12. Clones as a separate, bypassable final pass (decided)

Copies of the performer, cut out with the subject mask, are placed on
alternating sides, each rank smaller and dimmer (scaled around the bottom
edge so they share a floor line). They are a separate GLSL pass after
`look` rather than part of `mosh`, so they can be unplugged with
TouchDesigner's bypass flag. A bypassed TOP passes its first input
through, so no rewiring is needed. This matters for the intended
contexts: with one performer, clones turn a solo into a group; on a
real dance floor with many people, the mask already contains many
figures and clones would only clutter the frame.

Clones copy the *finished* image, so each clone carries the same
outlines and glow as the performer.

**Revision: a receding crowd with a time ripple.** Clones are laid out in
rows with pinhole perspective: row r at depth z = 1 + r * uRowDepth is
scaled by 1/z towards a vanishing point, and deeper rows hold more
figures, so the crowd widens with distance. Brightness falls off as
exp(-fog * (z - 1)). Each row shows the performer a few frames later than
the row in front, a canon effect. Past frames come from two Texture 3D TOPs
(image and mask, so silhouettes stay matched) used as 2D texture arrays.
They are ring buffers, and their write slot is set explicitly to
frame mod N, so the shader can compute which slice holds the frame from d
frames ago without knowing TouchDesigner's internal write index.

---

## D13. Video files: the audio is the master clock (decided)

To run a show recording, the service plays the file's audio and analyses
exactly the samples it hands to the sound card; each OSC bundle carries
the playback position (`/moshviz/time`). The Movie File In TOP is set to
*Specify Index* in seconds, following that value. This is how media
players keep A/V sync: the audio device's clock is steady and drift-free,
while video frames can be repeated or skipped without being noticed.
Two independent players (TouchDesigner's movie clock and the service's
timer) would drift apart over a clip.

Audio is decoded from the video container with PyAV, so no separate
extraction step and no system ffmpeg are needed.

**Follow-up: intra-frame video.** Following the audio clock means
seeking the movie every frame. The iPhone HEVC clip froze after a few
seconds while the audio clock kept running (confirmed in the service
log). Inter-frame codecs store occasional full frames and predict the
rest, so each seek decodes forward from the last full frame.
`tools/intra_frame.py` converts the video to ProRes Proxy, where every
frame is stored whole, at 1280 × 720 (the licence cap). It keeps the
source timestamps (1/600 s clock), so the variable frame rate is
preserved. The 11.8 s clip is 8 MB. MediaPipe on macOS only
takes a webcam, so file input runs without a performer mask, and the
clones pass is bypassed. The project cook rate is set to the clip's frame
rate (25.63 fps), for the same reason as the 30 fps camera lock.

---

## D14. Illumination compensation in the motion pass; exposure lift for dark footage (decided)

**Observed** on a show recording: the visuals reacted almost only when the
white stage lights flashed. Two causes:

1. *Brightness constancy fails under stage lighting.* Lucas-Kanade
   assumes a point keeps its brightness between frames. A strobe scales
   the whole frame's brightness, so the temporal derivative is large
   everywhere and gets read as motion, while real movement in the dark
   barely registers.
2. *Edges below threshold.* The outline threshold was tuned on a lit
   webcam image; in dark footage, gradients are too small.

**Decision.**
- `motion.glsl` estimates a per-block gain g = mean(current) /
  mean(previous) over its 16 samples and differences the current frame
  against g × previous. This is a standard robustness fix (normalising out
  a local brightness change). `uIlluminationComp` blends it in. Evidence
  (`td/reference/test_motion_reference.py`): a static scene with a 60 %
  flash reads as more than 0.3 px of motion without compensation and
  under 0.05 px with it, and a real (1, -0.5) px shift during the flash
  is still recovered within 0.15 px.
- `stylize.glsl` lifts luma before edge detection (`uExposure`,
  `uGamma`), with per-source values: movie footage uses exposure 2,
  gamma 0.6 and a lower threshold (`SOURCE_OVERRIDES` in
  `scripts/uniforms.py`).
- The motion block size follows the source resolution (`BLOCK_PX`).
  (TouchDesigner's Non-Commercial licence caps TOPs at 1280 × 1280, so the
  1920 × 1080 clip is processed at 1280 × 720, which is also why it looks
  softer when shown large.)

**Follow-up: relative edges.** Measured brightness through the chain:
the clip averages 0.031, but the outlines averaged 0.0019, so almost no
edges passed even with exposure lifted. Fixed gains are a poor fit for
stage light (spotlights next to near-black), so `stylize.glsl` can divide
the gradient by the local mean brightness (Weber contrast) plus a noise
floor (`uRelativeEdges`, `uContrastFloor`). Edges in shadows then count as
much as edges under a spotlight. Movie mode uses relative edges and shows
the lifted footage faintly under the outlines (`uImageLevel` 0.25).

---

## D8. Repo location (provisional)

`moshviz/` lives as a top-level folder in AudioDevProjects. That follows
the repo convention of self-contained per-project folders, and it keeps
the YIN source it ports one branch away. It can be split into its own repo
with `git subtree split` if a standalone link reads better on applications.
