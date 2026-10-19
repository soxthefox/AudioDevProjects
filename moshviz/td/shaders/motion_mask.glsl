// moshviz motion mask: "whatever moves" as a subject mask.
//
// GLSL TOP setup (created by scripts/wiring.py):
//   input 0: motion (per-block motion vectors from motion.glsl, 32-bit float)
//   Output resolution: the source size (set by wiring.py).
// Used where no person segmentation exists (movie mode on macOS): the
// clones pass copies the moving parts of the frame, which on a stage or a
// dance floor are mostly people. Output: white = moving.
//
// Each pixel blends the motion *speed* of the four nearest blocks
// bilinearly (blending speeds rather than vectors, so opposite motions in
// neighbouring blocks do not cancel), then maps it to 0..1 with a
// smoothstep between uSpeedLowPx and uSpeedHighPx.

uniform float uSpeedLowPx;    // speeds (px/frame) at or below this are background (e.g. 0.4)
uniform float uSpeedHighPx;   // speeds at or above this are fully "subject" (e.g. 1.5)

const int MOTION = 0;

out vec4 fragColor;

float blockSpeed(ivec2 block, ivec2 maxIndex)
{
    return length(texelFetch(sTD2DInputs[MOTION], clamp(block, ivec2(0), maxIndex), 0).xy);
}

void main()
{
    vec2 size = uTD2DInfos[MOTION].res.zw;
    vec2 p = vUV.st * size - 0.5;     // position in block-centre coordinates
    ivec2 i0 = ivec2(floor(p));
    vec2 f = p - floor(p);
    ivec2 maxIndex = ivec2(size) - 1;
    float speed = mix(mix(blockSpeed(i0, maxIndex), blockSpeed(i0 + ivec2(1, 0), maxIndex), f.x),
                      mix(blockSpeed(i0 + ivec2(0, 1), maxIndex), blockSpeed(i0 + ivec2(1, 1), maxIndex), f.x),
                      f.y);
    float subject = smoothstep(uSpeedLowPx, max(uSpeedHighPx, uSpeedLowPx + 1e-3), speed);
    fragColor = TDOutputSwizzle(vec4(vec3(subject), 1.0));
}
