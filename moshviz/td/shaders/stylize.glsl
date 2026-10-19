// moshviz stylize pass: turn the camera image into a lidar-like outline view.
//
// GLSL TOP setup (three wired inputs):
//   input 0: cam    (live camera frame)
//   input 1: motion (per-block motion vectors from motion.glsl, 32-bit float)
//   input 2: mask   (subject mask, white = performer)
//   Pixel Format: 16-bit float (RGBA). Output resolution: same as input 0.
// Its output replaces `cam` as mosh input 0 (and mosh_fb's input), so the
// datamosh drags outlines instead of the photographic image. The motion pass
// still analyses the real camera, which has the texture tracking needs.
//
// Per pixel:
//   0. Luma is lifted first: multiplied by uExposure, then raised to uGamma
//      (< 1 brightens shadows). Dark stage footage otherwise has gradients
//      too small to pass the edge threshold, so only light flashes showed.
//   1. Edge strength from a Sobel filter on luma, thresholded so camera noise
//      in dark areas does not sparkle. With uRelativeEdges = 1 the gradient is
//      divided by the local mean brightness (Weber contrast), so a step from
//      0.02 to 0.04 in a dark corner counts like 0.4 to 0.8 under a spotlight.
//      uContrastFloor is added to the mean so near-black noise is not
//      amplified. Stage footage averaging 3 % brightness gave almost no
//      absolute edges at all.
//   2. Brightness = edge * mix(uStaticLevel, 1, motion), so moving outlines
//      are bright and static ones dim: the view is about movement, not the room.
//   3. Optional dot grid, so outlines read as scanner points.
//   4. Line colour from the mask: one colour for the performer, one for the
//      room, a depth-like separation without a depth camera. uLineTint blends
//      the line colour towards the hue of the scene at that point, so stage
//      lighting colours the outlines.
//   5. Light layer: only pixels brighter than uLightThreshold (spotlights,
//      strobes, LEDs), in their original colour, averaged over a ring of taps
//      for a soft glow. It brings stage lighting back without bringing back
//      the photographic image.

uniform float uExposure;        // luma gain before edge detection (1 = unchanged; e.g. 2 for dark footage)
uniform float uGamma;           // luma gamma after exposure (1 = unchanged; < 1 lifts shadows, e.g. 0.6)
uniform float uRelativeEdges;   // 0 = absolute gradient, 1 = gradient / local brightness (Weber contrast)
uniform float uContrastFloor;   // added to the local mean before dividing; larger = less noise in the dark (e.g. 0.05)
uniform float uEdgeScalePx;     // Sobel tap spacing in pixels; larger = thicker, smoother lines (e.g. 1.5)
uniform float uEdgeGain;        // multiplies the gradient magnitude before thresholding (e.g. 4)
uniform float uEdgeThreshold;   // gradient below this (after gain) counts as no edge (e.g. 0.2)
uniform float uStaticLevel;     // brightness of outlines that are not moving, 0..1 (e.g. 0.2)
uniform float uMotionScalePx;   // speed (px/frame) that gives full brightness (e.g. 2)
uniform float uMotionFloorPx;   // speeds below this count as still (e.g. 0.3)
uniform float uPointSpacingPx;  // dot grid spacing in pixels; 0 = continuous lines (e.g. 5)
uniform float uPointMix;        // 0 = continuous lines, 1 = lines made only of dots
uniform float uImageLevel;      // how much of the (exposure-lifted) image shows under the lines (0 = none)
uniform vec3  uSubjectColour;   // outline colour on the performer
uniform vec3  uRoomColour;      // outline colour on everything else
uniform float uLineTint;        // 0 = fixed line colours, 1 = lines take the scene's local hue
uniform float uLightLevel;      // strength of the light layer (0 = off)
uniform float uLightThreshold;  // raw luma above which a pixel counts as a light (e.g. 0.45)
uniform float uLightGlowPx;     // radius of the light glow in pixels (e.g. 8)

const int CAMERA = 0;
const int MOTION = 1;
const int MASK = 2;
const vec3 REC709_LUMA = vec3(0.2126, 0.7152, 0.0722);
const float SOBEL_UNIT_STEP = 4.0;  // Sobel response to a 0 -> 1 step; dividing by it maps edges to ~0..1
const float EDGE_SOFTNESS = 0.1;    // width of the threshold ramp, so lines fade in rather than pop
const float DOT_RADIUS = 0.3;       // dot radius as a fraction of the grid cell
const float DOT_SOFTNESS = 0.1;     // anti-aliasing width of the dot edge, in cell units
const float LIGHT_SOFTNESS = 0.15;  // width of the light threshold ramp, in luma
const int GLOW_TAPS = 8;            // taps on the glow ring (plus the centre)
const float TAU = 6.28318530718;

out vec4 fragColor;

vec3 lifted(vec3 rgb)
{
    return pow(clamp(rgb * uExposure, 0.0, 1.0), vec3(max(uGamma, 1e-3)));
}

float luma(vec2 uv)
{
    float y = dot(texture(sTD2DInputs[CAMERA], uv).rgb, REC709_LUMA);
    return pow(clamp(y * uExposure, 0.0, 1.0), max(uGamma, 1e-3));
}

// Gradient magnitude from a 3x3 Sobel kernel, normalised so a hard
// black-to-white edge gives about 1, optionally relative to the local mean.
float sobelMagnitude(vec2 uv)
{
    vec2 d = uEdgeScalePx * uTD2DInfos[CAMERA].res.xy;
    float tl = luma(uv + vec2(-d.x,  d.y)), t = luma(uv + vec2(0.0,  d.y)), tr = luma(uv + vec2(d.x,  d.y));
    float l  = luma(uv + vec2(-d.x, 0.0)),                                  r  = luma(uv + vec2(d.x, 0.0));
    float bl = luma(uv + vec2(-d.x, -d.y)), b = luma(uv + vec2(0.0, -d.y)), br = luma(uv + vec2(d.x, -d.y));
    float gx = (tr + 2.0 * r + br) - (tl + 2.0 * l + bl);
    float gy = (tl + 2.0 * t + tr) - (bl + 2.0 * b + br);
    float absolute = length(vec2(gx, gy)) / SOBEL_UNIT_STEP;
    float localMean = (tl + t + tr + l + luma(uv) + r + bl + b + br) / 9.0;
    float relative = absolute / (localMean + max(uContrastFloor, 1e-3));
    return mix(absolute, relative, clamp(uRelativeEdges, 0.0, 1.0));
}

// 0 for still, 1 for moving at uMotionScalePx or faster. Linear filtering
// smooths the per-block field so brightness does not show block edges.
float motionEmphasis(vec2 uv)
{
    float speed = length(texture(sTD2DInputs[MOTION], uv).xy);
    return smoothstep(uMotionFloorPx, max(uMotionScalePx, uMotionFloorPx + 1e-3), speed);
}

// 1 on the dot grid, 0 between dots.
float dotGrid(vec2 uv)
{
    if (uPointSpacingPx <= 0.0) {
        return 1.0;
    }
    vec2 px = uv * uTD2DInfos[CAMERA].res.zw;
    float distanceToDot = length(fract(px / uPointSpacingPx) - 0.5);
    return 1.0 - smoothstep(DOT_RADIUS, DOT_RADIUS + DOT_SOFTNESS, distanceToDot);
}

// The pixel's colour if it is bright enough to count as a light, else black.
vec3 lightAt(vec2 uv)
{
    vec3 rgb = texture(sTD2DInputs[CAMERA], uv).rgb;
    float y = dot(rgb, REC709_LUMA);
    return rgb * smoothstep(uLightThreshold, uLightThreshold + LIGHT_SOFTNESS, y);
}

// Lights plus a soft glow: the centre and a ring of GLOW_TAPS taps at
// uLightGlowPx, averaged (a cheap blur, enough for a halo).
vec3 lightGlow(vec2 uv)
{
    vec2 radius = uLightGlowPx * uTD2DInfos[CAMERA].res.xy;
    vec3 sum = lightAt(uv);
    for (int i = 0; i < GLOW_TAPS; ++i) {
        float angle = TAU * float(i) / float(GLOW_TAPS);
        sum += lightAt(uv + radius * vec2(cos(angle), sin(angle)));
    }
    return sum / float(GLOW_TAPS + 1);
}

// Hue of the scene at uv, at full brightness (largest channel = 1).
vec3 sceneHue(vec2 uv)
{
    vec3 rgb = lifted(texture(sTD2DInputs[CAMERA], uv).rgb);
    return rgb / max(max(rgb.r, max(rgb.g, rgb.b)), 1e-3);
}

void main()
{
    vec2 uv = vUV.st;

    float edge = smoothstep(uEdgeThreshold, uEdgeThreshold + EDGE_SOFTNESS,
                            sobelMagnitude(uv) * uEdgeGain);
    float brightness = edge * mix(uStaticLevel, 1.0, motionEmphasis(uv));
    brightness *= mix(1.0, dotGrid(uv), uPointMix);

    float subject = texture(sTD2DInputs[MASK], uv).r;
    vec3 lineColour = mix(uRoomColour, uSubjectColour, subject);
    lineColour = mix(lineColour, sceneHue(uv), clamp(uLineTint, 0.0, 1.0));
    vec3 photo = lifted(texture(sTD2DInputs[CAMERA], uv).rgb) * uImageLevel;
    vec3 lights = uLightLevel > 0.0 ? lightGlow(uv) * uLightLevel : vec3(0.0);

    fragColor = TDOutputSwizzle(vec4(photo + lights + lineColour * brightness, 1.0));
}
