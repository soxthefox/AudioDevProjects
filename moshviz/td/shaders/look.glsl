// moshviz look pass: colour grade plus an audio-reactive motion glow.
//
// GLSL TOP setup:
//   input 0: mosh  (RGB = moshed image, A = per-block audio amount)
//   input 1: motion (per-block motion vectors, 32-bit float)
//   Pixel Format: 16-bit float (RGBA). Output resolution: same as input 0.
//
// The glow draws the motion field the way optical-flow papers do (Baker et
// al., "A Database and Evaluation Methodology for Optical Flow", 2011): hue
// = direction of motion, brightness = speed. Each macroblock is a flat
// tile, so the glow shows the same blocks the datamosh moves.

uniform float uBrightness;      // added after contrast, e.g. -0.1 for a darker base
uniform float uContrast;        // 1 = unchanged; > 1 = punchier
uniform float uSaturation;      // 1 = unchanged; 0 = greyscale
uniform float uDuotone;         // 0 = natural colour; 1 = fully mapped to the two tint colours
uniform vec3  uShadowTint;      // duotone colour for dark tones, e.g. (0.05, 0.0, 0.15)
uniform vec3  uHighlightTint;   // duotone colour for bright tones, e.g. (1.0, 0.75, 0.6)
uniform float uMotionGlow;      // glow strength with no audio; 0 = off
uniform float uGlowAudio;       // extra glow per unit of audio amount
uniform float uMotionScalePx;   // speed (px/frame) shown at full brightness, e.g. 3
uniform float uMotionFloorPx;   // speeds below this show no glow (hides camera noise), e.g. 0.3
uniform float uGlowSmooth;      // 0 = one flat colour per macroblock, 1 = bilinear blend between blocks

const int IMAGE = 0;
const int MOTION = 1;
const vec3 REC709_LUMA = vec3(0.2126, 0.7152, 0.0722);
const float MID_GREY = 0.5;       // pivot for the contrast curve
const float TAU = 6.28318530718;

out vec4 fragColor;

vec3 grade(vec3 c)
{
    float luma = dot(c, REC709_LUMA);
    c = mix(vec3(luma), c, uSaturation);
    c = (c - MID_GREY) * uContrast + MID_GREY + uBrightness;
    c = max(c, 0.0);
    vec3 duotone = mix(uShadowTint, uHighlightTint, clamp(dot(c, REC709_LUMA), 0.0, 1.0));
    return mix(c, duotone, uDuotone);
}

// Fully saturated colour for a hue in [0, 1): a cosine approximation of the
// HSV hue wheel, smooth enough for a visualisation.
vec3 hueColour(float hue)
{
    return 0.5 + 0.5 * cos(TAU * (hue + vec3(0.0, -1.0 / 3.0, 1.0 / 3.0)));
}

// Motion vector of the block containing uv (flat tiles).
vec2 blockVector(vec2 uv)
{
    return texelFetch(sTD2DInputs[MOTION], ivec2(uv * uTD2DInfos[MOTION].res.zw), 0).xy;
}

// Motion vector blended bilinearly from the four nearest block centres.
// Done by hand with texelFetch, so it does not rely on the GPU filtering
// 32-bit float textures.
vec2 smoothVector(vec2 uv)
{
    vec2 size = uTD2DInfos[MOTION].res.zw;
    vec2 p = uv * size - 0.5;            // position in block-centre coordinates
    ivec2 i0 = ivec2(floor(p));
    vec2 f = p - floor(p);
    ivec2 maxIndex = ivec2(size) - 1;
    vec2 v00 = texelFetch(sTD2DInputs[MOTION], clamp(i0,               ivec2(0), maxIndex), 0).xy;
    vec2 v10 = texelFetch(sTD2DInputs[MOTION], clamp(i0 + ivec2(1, 0), ivec2(0), maxIndex), 0).xy;
    vec2 v01 = texelFetch(sTD2DInputs[MOTION], clamp(i0 + ivec2(0, 1), ivec2(0), maxIndex), 0).xy;
    vec2 v11 = texelFetch(sTD2DInputs[MOTION], clamp(i0 + ivec2(1, 1), ivec2(0), maxIndex), 0).xy;
    return mix(mix(v00, v10, f.x), mix(v01, v11, f.x), f.y);
}

vec3 motionGlow(vec2 uv, float audioAmount)
{
    vec2 v = mix(blockVector(uv), smoothVector(uv), clamp(uGlowSmooth, 0.0, 1.0));
    float speed = length(v);
    float visible = smoothstep(uMotionFloorPx, max(uMotionScalePx, uMotionFloorPx + 1e-3), speed);
    float hue = atan(v.y, v.x) / TAU + 0.5;   // direction -> 0..1
    float strength = uMotionGlow + uGlowAudio * audioAmount;
    return hueColour(hue) * visible * strength;
}

void main()
{
    vec2 uv = vUV.st;
    vec4 moshed = texture(sTD2DInputs[IMAGE], uv);
    // Additive glow: light added on top of the graded image, as a screen would.
    vec3 colour = grade(moshed.rgb) + motionGlow(uv, moshed.a);
    fragColor = TDOutputSwizzle(vec4(colour, 1.0));
}
