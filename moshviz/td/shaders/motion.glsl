// moshviz motion pass: one motion vector per macroblock.
//
// GLSL TOP setup:
//   input 0: current camera frame
//   input 1: previous camera frame (Feedback TOP targeting the camera Null)
//   Output Resolution: camera resolution divided by the block size,
//     e.g. 80 x 60 for 8 px blocks on 640 x 480. The block size is not a
//     uniform: it is input resolution / output resolution.
//   Pixel Format: 32-bit float (RGBA). Motion is signed and in pixels.
//
// Output texel = (vx, vy, confidence, 1), with motion in input pixels per frame.
//
// Method: Lucas-Kanade over the block. Brightness constancy gives, at each
// sample, Ix*vx + Iy*vy + It = 0. Least squares over the samples in the block
// solves
//     [sum Ix^2   sum IxIy] [vx]     [sum IxIt]
//     [sum IxIy   sum Iy^2] [vy] = - [sum IyIt]
// A small ridge term (uRegularisation) keeps flat, textureless blocks at
// v ~ 0 instead of blowing up. One vector per block mirrors how a codec
// stores motion, and is what makes the final mosh look blocky.

uniform float uRegularisation;   // ridge term added to the 2x2 diagonal (luma^2 units). Try 1e-3.

const int CURRENT = 0;
const int PREVIOUS = 1;
const int SAMPLES_PER_AXIS = 4;  // 4 x 4 = 16 LK samples per block
const float MIN_REGULARISATION = 1e-6;  // keeps det > 0 even if the uniform is unset (0)
const vec3 REC709_LUMA = vec3(0.2126, 0.7152, 0.0722);

out vec4 fragColor;

float luma(int inputIndex, vec2 uv)
{
    return dot(texture(sTD2DInputs[inputIndex], uv).rgb, REC709_LUMA);
}

void main()
{
    vec2 inTexel = uTD2DInfos[CURRENT].res.xy;              // 1 / input size
    vec2 blockPx = uTD2DInfos[CURRENT].res.zw / uTDOutputInfo.res.zw;
    vec2 blockOriginPx = floor(vUV.st * uTDOutputInfo.res.zw) * blockPx;

    // Samples sit at the centres of a SAMPLES_PER_AXIS grid inside the block.
    // The gradient uses central differences one sample-spacing apart, which
    // acts like a gradient of a slightly blurred image and is less noisy.
    vec2 spacingPx = blockPx / float(SAMPLES_PER_AXIS);
    vec2 dx = vec2(spacingPx.x, 0.0) * inTexel;
    vec2 dy = vec2(0.0, spacingPx.y) * inTexel;

    float sIxx = 0.0, sIxy = 0.0, sIyy = 0.0, sIxt = 0.0, sIyt = 0.0;
    for (int j = 0; j < SAMPLES_PER_AXIS; ++j) {
        for (int i = 0; i < SAMPLES_PER_AXIS; ++i) {
            vec2 uv = (blockOriginPx + (vec2(i, j) + 0.5) * spacingPx) * inTexel;
            // Divide by the spacing so gradients are per pixel, giving v in pixels.
            float Ix = (luma(CURRENT, uv + dx) - luma(CURRENT, uv - dx)) / (2.0 * spacingPx.x);
            float Iy = (luma(CURRENT, uv + dy) - luma(CURRENT, uv - dy)) / (2.0 * spacingPx.y);
            float It = luma(CURRENT, uv) - luma(PREVIOUS, uv);
            sIxx += Ix * Ix;  sIxy += Ix * Iy;  sIyy += Iy * Iy;
            sIxt += Ix * It;  sIyt += Iy * It;
        }
    }

    float ridge = max(uRegularisation, MIN_REGULARISATION);
    float a = sIxx + ridge;
    float d = sIyy + ridge;
    float det = a * d - sIxy * sIxy;   // > 0 because ridge > 0
    // Closed-form 2x2 inverse: [d -b; -b a] / det.
    vec2 v = -vec2(d * sIxt - sIxy * sIyt, a * sIyt - sIxy * sIxt) / det;

    // Confidence: smaller eigenvalue of the structure tensor (Shi-Tomasi). Near
    // 0 on edges and flat areas, where motion is ambiguous.
    float trace = sIxx + sIyy;
    float minEigen = 0.5 * (trace - sqrt(max(trace * trace - 4.0 * (sIxx * sIyy - sIxy * sIxy), 0.0)));

    fragColor = vec4(v, minEigen, 1.0);
}
