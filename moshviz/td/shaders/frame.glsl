// moshviz frame pass: fixed output canvas, filled with mirrored copies.
//
// GLSL TOP setup (created by scripts/wiring.py):
//   input 0: clones_switch (the finished image, any size or aspect)
//   Output resolution: fixed (OUTPUT_SIZE in wiring.py, 1280 x 720).
// Its output feeds `out`, so the piece always has the same shape whatever
// the source (portrait phone clips, the landscape webcam).
//
// Fit. The image is scaled uniformly to fit inside the canvas ("contain"),
// times uFitZoom, and centred. In image coordinates (0..1 = the image),
// each canvas pixel lands at
//     local = (uv - 0.5) * canvasSize / (imageSize * scale) + 0.5.
//
// Mirror fill. Coordinates outside 0..1 are folded back with a triangle
// wave, so the copies alternate between flipped and unflipped, and every
// seam joins up with no visible edge. Copy number k (0 = the image itself)
// is darkened by uMirrorFade^|k|, keeping the centre the focus.
//
// Symmetry. Optionally the left half is mirrored onto the right, around
// the image's centre line, so the whole picture is symmetric: with the
// datamosh and motion glow on top, this reads as a kaleidoscope.

uniform float uFitZoom;      // 1 = fit inside the canvas; > 1 zooms in (mirrors continue the image)
uniform float uMirrorFill;   // 1 = fill the canvas with mirrored copies, 0 = black outside the image
uniform float uMirrorFade;   // brightness of each successive copy, 0..1 (e.g. 0.6)
uniform float uSymmetry;     // 1 = mirror the left half onto the right, 0 = off

const int IMAGE = 0;

out vec4 fragColor;

// Folds any coordinate into 0..1 with alternating flips: 1.2 -> 0.8, -0.3 -> 0.3.
float mirrored(float x)
{
    return 1.0 - abs(mod(x, 2.0) - 1.0);
}

void main()
{
    vec2 canvas = uTDOutputInfo.res.zw;
    vec2 image = uTD2DInfos[IMAGE].res.zw;
    float scale = min(canvas.x / image.x, canvas.y / image.y) * max(uFitZoom, 1e-3);
    vec2 local = (vUV.st - 0.5) * canvas / (image * scale) + 0.5;

    if (uSymmetry > 0.5) {
        local.x = 0.5 - abs(local.x - 0.5);
    }

    vec2 copyIndex = floor(local);                  // (0, 0) = the image itself
    bool inside = all(equal(copyIndex, vec2(0.0)));
    if (!inside && uMirrorFill < 0.5) {
        fragColor = TDOutputSwizzle(vec4(0.0, 0.0, 0.0, 1.0));
        return;
    }

    vec2 src = vec2(mirrored(local.x), mirrored(local.y));
    float fade = pow(clamp(uMirrorFade, 0.0, 1.0), abs(copyIndex.x) + abs(copyIndex.y));
    vec3 colour = texture(sTD2DInputs[IMAGE], src).rgb * fade;
    fragColor = TDOutputSwizzle(vec4(colour, 1.0));
}
