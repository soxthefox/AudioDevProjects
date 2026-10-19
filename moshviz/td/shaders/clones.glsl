// moshviz clones pass (optional): a crowd of copies of the performer,
// in rows receding to a vanishing point.
//
// GLSL TOP setup (three wired inputs):
//   input 0: look           (the finished image)
//   input 1: history_image  (Texture 3D TOP of `look`, as a 2D texture array)
//   input 2: history_mask   (Texture 3D TOP of `mask`, as a 2D texture array)
//   Pixel Format: 16-bit float (RGBA).
//   TOPs parameter: mask  (the live subject mask, input 3)
// GLSL TOP sorts inputs by texture type, so `look` is sTD2DInputs[0], the
// live mask sTD2DInputs[1], and the two histories sTD2DArrayInputs[0] and
// [1]. scripts/wiring.py creates the history nodes and sets all of this.
// Sits between `look` and `out`. To unplug it, bypass the node (Textport:
// op('/project1/clones').bypass = True) or set uRows to 0. A bypassed TOP
// passes input 0 through unchanged, so nothing else needs rewiring. Meant
// for a single performer; with a real crowd, unplug it.
//
// Perspective model. Row r stands at depth z = 1 + r * uRowDepth (the
// performer's row is z = 1). A pinhole camera scales everything at depth z
// by 1/z towards the vanishing point, so in row r:
//   - figures are 1/z the size, scaled around their feet;
//   - their feet sit at height uHorizon * (1 - 1/z), rising towards the
//     horizon as z grows;
//   - column c stands c * uCloneSpacing / z from the centre line, so rows
//     narrow towards the vanishing point at (0.5, uHorizon).
// A deeper row fits more figures across the frame, so up to uColumns per
// side are drawn and off-screen ones skipped. Brightness falls off as
// exp(-uFog * (z - 1)), so far rows dissolve instead of ending abruptly.
// Rows are drawn far to near, with the performer on top.
//
// Time delay. Each history node writes the current frame into slice
// (frame mod N), where N is its cache size, so the frame from d frames ago
// is in slice (frame - d) mod N. A clone in row r, column c shows the
// frame from 1 + r * uRowDelayFrames + |c| * uColumnDelayFrames ago, so
// the crowd repeats the performer's movement in a ripple running back
// through the rows. The extra frame keeps clones off the slice being
// written this frame: whether the history node cooks before or after this
// shader reads the clock, slice (frame - 1) holds a recent, complete frame.
// (Reading delay 0 from history to protect the performer once picked up a
// frame from a second ago and let clones cover the performer's hand; the
// performer's priority now uses the live mask.)
//
// Blending. uBlend = 1 ("lighten") takes the brighter of the clone and
// what is behind it, so clones only ever add light. On the dark outline
// image, the performer's lines stay visible even where segmentation misses
// a fast-moving hand. uBlend = 0 makes clones opaque, with nearer figures
// covering further ones.

uniform float uRows;          // rows of clones including the performer's row, 0 to MAX_ROWS (0 = off)
uniform float uColumns;       // clones per side in each row, 0 to MAX_COLUMNS
uniform float uCloneSpacing;  // distance between neighbours in the front row, as a fraction of frame width (e.g. 0.3)
uniform float uRowDepth;      // depth added per row (e.g. 0.6); larger = rows shrink faster
uniform float uHorizon;       // height of the vanishing point, 0 = bottom, 1 = top (e.g. 0.65)
uniform float uFog;           // brightness falloff with depth (e.g. 0.8); 0 = no fade
uniform float uMaskFeather;   // softness of the cut-out edge, in mask units (e.g. 0.1)
uniform float uRowDelayFrames;    // extra delay per row, in frames (e.g. 3; 0 = all in sync)
uniform float uColumnDelayFrames; // extra delay per column away from the centre, in frames (e.g. 1)
uniform float uHistoryFrames;     // cache size of the history nodes (frames kept)
uniform float uFrame;             // absTime.frame, the same clock the history nodes use
uniform float uBlend;             // 1 = lighten (clones add light only), 0 = opaque clones

const int IMAGE = 0;            // sTD2DInputs index
const int LIVE_MASK = 1;        // sTD2DInputs index (from the TOPs parameter)
const float MIN_DELAY_FRAMES = 1.0;
const int HISTORY_IMAGE = 0;    // sTD2DArrayInputs index
const int HISTORY_MASK = 1;     // sTD2DArrayInputs index
const int MAX_ROWS = 10;
const int MAX_COLUMNS = 8;
const float FEET_X = 0.5;     // the performer stands at the horizontal centre
const float FEET_Y = 0.0;     // and is scaled around the bottom edge of the frame

out vec4 fragColor;

// History slice holding the frame from `delay` frames ago (at least
// MIN_DELAY_FRAMES; see the time-delay note above).
float historySlice(float delay)
{
    float n = max(uHistoryFrames, 2.0);
    float d = clamp(floor(delay + 0.5) + MIN_DELAY_FRAMES, MIN_DELAY_FRAMES, n - 1.0);
    return mod(uFrame - d, n);
}

float feathered(float maskValue)
{
    return smoothstep(0.5 - uMaskFeather, 0.5 + uMaskFeather, maskValue);
}

// Clone coverage at `uv` in the historical frame `slice`.
float cloneAt(vec2 uv, float slice)
{
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
        return 0.0;
    }
    return feathered(texture(sTD2DArrayInputs[HISTORY_MASK], vec3(uv, slice)).r);
}

// Screen position of the feet of the figure in column `column` at depth `z`.
vec2 feetOnScreen(float column, float z)
{
    return vec2(FEET_X + column * uCloneSpacing / z,
                FEET_Y + (uHorizon - FEET_Y) * (1.0 - 1.0 / z));
}

// Inverse of the placement: the point in the original frame that this
// screen pixel shows, for the figure standing at `feet` with depth `z`.
vec2 sourceUV(vec2 uv, vec2 feet, float z)
{
    return vec2(FEET_X, FEET_Y) + (uv - feet) * z;
}

void main()
{
    vec2 uv = vUV.st;
    vec4 base = texture(sTD2DInputs[IMAGE], uv);
    vec3 colour = base.rgb;

    int rows = clamp(int(uRows + 0.5), 0, MAX_ROWS);
    int columns = clamp(int(uColumns + 0.5), 0, MAX_COLUMNS);

    for (int row = rows - 1; row >= 0; --row) {
        float z = 1.0 + float(row) * uRowDepth;
        float brightness = exp(-uFog * (z - 1.0));
        for (int c = -columns; c <= columns; ++c) {
            if (row == 0 && c == 0) {
                continue;  // the performer's own spot
            }
            vec2 feet = feetOnScreen(float(c), z);
            vec2 src = sourceUV(uv, feet, z);
            float slice = historySlice(float(row) * uRowDelayFrames
                                       + float(abs(c)) * uColumnDelayFrames);
            float coverage = cloneAt(src, slice);
            if (coverage > 0.0) {
                vec3 figure = texture(sTD2DArrayInputs[HISTORY_IMAGE], vec3(src, slice)).rgb * brightness;
                vec3 opaque = mix(colour, figure, coverage);
                vec3 lighten = max(colour, figure * coverage);
                colour = mix(opaque, lighten, uBlend);
            }
        }
    }

    // The performer stays in front of every clone, using the live mask.
    colour = mix(colour, base.rgb, feathered(texture(sTD2DInputs[LIVE_MASK], uv).r));
    fragColor = TDOutputSwizzle(vec4(colour, 1.0));
}
