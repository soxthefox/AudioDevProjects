// moshviz datamosh pass.
//
// GLSL TOP inputs. A GLSL TOP takes at most three wired inputs; inputs 3
// and 4 are listed in its TOPs parameter ("rows motion"), which appends
// them after the wired ones.
//   0 CURRENT  live camera frame                                  (wired)
//   1 FEEDBACK previous output of this TOP (Feedback TOP)         (wired)
//   2 MASK     subject mask, white = performer                    (wired)
//   3 ROWS     1 x N mosh amounts per strip (CHOP to TOP)         (TOPs)
//   4 MOTION   per-block motion vectors from motion.glsl (float)  (TOPs)
//
// A real codec stores a keyframe, then predicts each later frame by moving
// blocks of the previous decoded frame along motion vectors. Datamoshing
// removes keyframes, so the "previous frame" goes stale and old pixels get
// dragged along new motion. This pass does that per strip: where a strip's
// amount is high, the output is the previous output displaced by the block
// motion, not the live frame. The subject mask always shows the live frame.
//
// Output alpha is the per-block audio amount, not opacity (see the end of main).

uniform float uDisplacement;  // gain on motion vectors (1 = codec-like; > 1 exaggerates)
uniform float uMaxMotionPx;   // clamp on per-frame displacement, in pixels
uniform float uRefresh;       // 0..1 pull of moshed pixels toward the live frame each frame
uniform float uMaskFeather;   // half-width of the mask edge ramp, in mask units (e.g. 0.1)
uniform float uMaskInvert;    // 1 if the mask source is white = background (MediaPipe background-only mode)
uniform float uThresholdSoftness; // 0 = hard on/off blocks; larger = blocks fade in and out (e.g. 0.05)
uniform float uReseedFrames;  // frames between reshuffles of which blocks mosh; 0 = never reshuffle
uniform float uFrame;         // set to absTime.frame
uniform float uRadial;        // 1 = concentric rings (low bands at the centre), 0 = horizontal strips
uniform float uRadialCurve;   // ring spacing: 2 = equal-area rings; > 2 = bigger centre (low) rings
uniform float uMotionDrive;   // 0 = band layout decides where (rings/strips), 1 = motion decides where
uniform float uMoshSpeedLowPx;  // with motion drive: block speeds at or below this do not mosh (e.g. 0.3)
uniform float uMoshSpeedHighPx; // with motion drive: speeds at or above this get the full audio amount (e.g. 2)
uniform float uMotionGain;    // with motion drive: multiplies the audio energy (e.g. 1.5)

const int CURRENT = 0;
const int FEEDBACK = 1;
const int MASK = 2;
const int ROWS = 3;
const int MOTION = 4;

out vec4 fragColor;

// Position of a point in band space, 0 (lowest band) to 1 (highest).
//
// Strips: the vertical position, so strip 0 is at the bottom of the frame
// (TouchDesigner's v = 0).
// Rings: the distance from the frame centre, corrected for aspect ratio and
// normalised so the corners are at 1. Raising it to the power uRadialCurve
// (k) sets the ring spacing. Ring i covers radii (i/N)^(1/k) to ((i+1)/N)^(1/k), so
// its area is proportional to ((i+1)/N)^(2/k) - (i/N)^(2/k). For k = 2,
// every ring has the same area; for k > 2, inner (low) rings are larger.
float bandPosition(vec2 uv)
{
    if (uRadial < 0.5) {
        return uv.y;
    }
    vec2 aspect = vec2(uTD2DInfos[CURRENT].res.z / uTD2DInfos[CURRENT].res.w, 1.0);
    float r = length((uv - 0.5) * aspect) / length(0.5 * aspect);
    return pow(clamp(r, 0.0, 1.0), max(uRadialCurve, 1e-3));
}

// Mosh amount for the band at `position` (0..1). The band count is the row
// texture's height, so it cannot drift out of sync with the feature
// service's band count.
float bandAmount(float position)
{
    float bandCount = uTD2DInfos[ROWS].res.w;
    float band = min(floor(position * bandCount), bandCount - 1.0);
    return texelFetch(sTD2DInputs[ROWS], ivec2(0, int(band)), 0).r;
}

// texelFetch rather than texture(): every pixel in a block gets exactly that
// block's vector, so moshed regions move as hard-edged macroblocks.
ivec2 blockCoord(vec2 uv)
{
    return ivec2(uv * uTD2DInfos[MOTION].res.zw);
}

// Overall audio energy: the mean of all band amounts (0..1).
float audioEnergy()
{
    int bandCount = int(uTD2DInfos[ROWS].res.w);
    float sum = 0.0;
    for (int band = 0; band < bandCount; ++band) {
        sum += texelFetch(sTD2DInputs[ROWS], ivec2(0, band), 0).r;
    }
    return sum / float(max(bandCount, 1));
}

// Motion-driven amount for a block: how fast it moves decides *where*, the
// overall audio energy decides *how much*. The moving blocks, the same ones
// look.glsl colours, are the ones that break, and silence still means none.
float motionAmount(ivec2 block)
{
    float speed = length(texelFetch(sTD2DInputs[MOTION], block, 0).xy);
    float moving = smoothstep(uMoshSpeedLowPx, max(uMoshSpeedHighPx, uMoshSpeedLowPx + 1e-3), speed);
    return clamp(moving * audioEnergy() * uMotionGain, 0.0, 1.0);
}

vec2 blockMotionPx(ivec2 block)
{
    vec2 v = texelFetch(sTD2DInputs[MOTION], block, 0).xy * uDisplacement;
    float len = length(v);
    return len > uMaxMotionPx ? v * (uMaxMotionPx / len) : v;
}

// Integer hash (PCG-style) giving a uniform value in [0, 1) per block and seed.
float blockRandom(ivec2 block, float seed)
{
    uint h = uint(block.x) * 1973u + uint(block.y) * 9277u + uint(seed) * 26699u;
    h = h * 747796405u + 2891336453u;
    h = ((h >> ((h >> 28u) + 4u)) ^ h) * 277803737u;
    h = (h >> 22u) ^ h;
    return float(h) / 4294967296.0;
}

// Which blocks show the stale image. Each block has a fixed random threshold;
// a block moshes fully while its strip's amount is above that threshold, so
// amount 0.5 means about half the blocks in the strip mosh, and they stay
// moshed frame after frame, like macroblocks that keep being predicted from
// a stale reference instead of being refreshed.
//
// Blending every pixel by `amount` instead (the first version) replaced
// part of the trail with the live frame on every frame, so smears faded
// within a few frames and the effect was barely visible.
float moshWeight(float amount, ivec2 block)
{
    float seed = uReseedFrames > 0.0 ? floor(uFrame / uReseedFrames) : 0.0;
    float threshold = blockRandom(block, seed);
    float halfWidth = max(uThresholdSoftness, 1e-4) * 0.5;
    return smoothstep(threshold - halfWidth, threshold + halfWidth, amount);
}

void main()
{
    vec2 uv = vUV.st;
    vec4 live = texture(sTD2DInputs[CURRENT], uv);

    ivec2 block = blockCoord(uv);
    vec2 motionUV = blockMotionPx(block) * uTD2DInfos[FEEDBACK].res.xy;
    // Pull from where the content was last frame: backward warp of the old output.
    vec4 stale = texture(sTD2DInputs[FEEDBACK], uv - motionUV);
    // A single NaN/Inf in a feedback loop never washes out (any mix with it is
    // NaN) and shows as black forever. Fall back to the live pixel instead.
    if (any(isnan(stale)) || any(isinf(stale))) {
        stale = live;
    }
    vec4 moshed = mix(stale, live, uRefresh);

    // Read the amount at the block centre so a block is never split
    // between two bands.
    vec2 blockCentreUV = (vec2(block) + 0.5) / uTD2DInfos[MOTION].res.zw;
    float bandDriven = bandAmount(bandPosition(blockCentreUV));
    float amount = mix(bandDriven, motionAmount(block), clamp(uMotionDrive, 0.0, 1.0));
    vec4 background = mix(live, moshed, moshWeight(amount, block));

    float maskValue = texture(sTD2DInputs[MASK], uv).r;
    maskValue = mix(maskValue, 1.0 - maskValue, uMaskInvert);
    float subject = smoothstep(0.5 - uMaskFeather, 0.5 + uMaskFeather, maskValue);
    // RGB is the image. Alpha carries this block's audio amount (0 on the
    // subject) for look.glsl, which uses it to drive the motion glow;
    // look.glsl sets alpha back to 1 for display and recording.
    vec3 colour = mix(background.rgb, live.rgb, subject);
    fragColor = TDOutputSwizzle(vec4(colour, amount * (1.0 - subject)));
}
