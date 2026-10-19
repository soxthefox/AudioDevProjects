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

uniform float uDisplacement;  // gain on motion vectors (1 = codec-like; > 1 exaggerates)
uniform float uMaxMotionPx;   // clamp on per-frame displacement, in pixels
uniform float uRefresh;       // 0..1 pull of moshed pixels toward the live frame each frame
uniform float uMaskFeather;   // half-width of the mask edge ramp, in mask units (e.g. 0.1)
uniform float uBlockDropout;  // 0 = smooth blend by amount, 1 = each block is all-or-nothing
uniform float uFrame;         // set to absTime.frame; reseeds the block dropout every frame

const int CURRENT = 0;
const int FEEDBACK = 1;
const int MASK = 2;
const int ROWS = 3;
const int MOTION = 4;

out vec4 fragColor;

// Mosh amount for the strip containing this pixel. Strip 0 is at the bottom
// of the frame (TouchDesigner's v = 0), so low guitar bands mosh low rows.
// The strip count is the row texture's height, so it cannot drift out of
// sync with the feature service's band count.
float stripAmount(float v)
{
    float stripCount = uTD2DInfos[ROWS].res.w;
    float strip = min(floor(v * stripCount), stripCount - 1.0);
    return texelFetch(sTD2DInputs[ROWS], ivec2(0, int(strip)), 0).r;
}

// texelFetch rather than texture(): every pixel in a block gets exactly that
// block's vector, so moshed regions move as hard-edged macroblocks.
ivec2 blockCoord(vec2 uv)
{
    return ivec2(uv * uTD2DInfos[MOTION].res.zw);
}

vec2 blockMotionPx(ivec2 block)
{
    vec2 v = texelFetch(sTD2DInputs[MOTION], block, 0).xy * uDisplacement;
    float len = length(v);
    return len > uMaxMotionPx ? v * (uMaxMotionPx / len) : v;
}

// Integer hash (PCG-style) giving a uniform value in [0, 1) per block per frame.
float blockRandom(ivec2 block, float frame)
{
    uint h = uint(block.x) * 1973u + uint(block.y) * 9277u + uint(frame) * 26699u;
    h = h * 747796405u + 2891336453u;
    h = ((h >> ((h >> 28u) + 4u)) ^ h) * 277803737u;
    h = (h >> 22u) ^ h;
    return float(h) / 4294967296.0;
}

// How much of the moshed (stale) image to show. With dropout, a block is
// moshed with probability `amount`, like a codec randomly losing keyframe
// refreshes per macroblock; without, it blends smoothly by `amount`.
float moshWeight(float amount, ivec2 block)
{
    float allOrNothing = step(blockRandom(block, uFrame), amount);
    return mix(amount, allOrNothing, uBlockDropout);
}

void main()
{
    vec2 uv = vUV.st;
    vec4 live = texture(sTD2DInputs[CURRENT], uv);

    ivec2 block = blockCoord(uv);
    vec2 motionUV = blockMotionPx(block) * uTD2DInfos[FEEDBACK].res.xy;
    // Pull from where the content was last frame: backward warp of the old output.
    vec4 stale = texture(sTD2DInputs[FEEDBACK], uv - motionUV);
    vec4 moshed = mix(stale, live, uRefresh);

    vec4 background = mix(live, moshed, moshWeight(stripAmount(uv.y), block));

    float subject = smoothstep(0.5 - uMaskFeather, 0.5 + uMaskFeather,
                               texture(sTD2DInputs[MASK], uv).r);
    fragColor = TDOutputSwizzle(mix(background, live, subject));
}
