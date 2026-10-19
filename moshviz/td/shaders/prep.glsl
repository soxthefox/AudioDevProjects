// moshviz prep pass: grade the input before anything analyses it.
//
// GLSL TOP setup (created by scripts/wiring.py):
//   input 0: the source video (camera via MediaPipe, or the movie)
//   output -> cam, which feeds motion, stylize and the rest.
// The uniforms follow the `controls` panel (scripts/uniforms.py binds them
// by expression), so the input can be adjusted live.
//
// Grading before analysis matters: the motion pass and the outline pass
// both work on brightness differences, so lifting dark footage and raising
// its contrast gives them more to find. Soften removes sensor noise first,
// so grain in dark areas is not mistaken for motion or edges.
//
// Order: soften -> exposure -> contrast (around mid grey) + brightness ->
// gamma -> saturation.

uniform float uExposure;    // multiplies brightness; 1 = unchanged
uniform float uBrightness;  // added after contrast; 0 = unchanged
uniform float uContrast;    // 1 = unchanged; > 1 spreads tones apart
uniform float uGamma;       // 1 = unchanged; > 1 lifts shadows
uniform float uSaturation;  // 1 = unchanged; 0 = greyscale
uniform float uSoften;      // 0 = sharp; 1 = full 3 x 3 box blur (noise reduction)

const int SOURCE = 0;
const vec3 REC709_LUMA = vec3(0.2126, 0.7152, 0.0722);
const float MID_GREY = 0.5;   // contrast pivot

out vec4 fragColor;

vec3 softened(vec2 uv)
{
    vec3 centre = texture(sTD2DInputs[SOURCE], uv).rgb;
    if (uSoften <= 0.0) {
        return centre;
    }
    vec2 px = uTD2DInfos[SOURCE].res.xy;
    vec3 sum = vec3(0.0);
    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            sum += texture(sTD2DInputs[SOURCE], uv + vec2(i, j) * px).rgb;
        }
    }
    return mix(centre, sum / 9.0, clamp(uSoften, 0.0, 1.0));
}

void main()
{
    vec3 c = softened(vUV.st) * uExposure;
    c = (c - MID_GREY) * uContrast + MID_GREY + uBrightness;
    c = pow(max(c, 0.0), vec3(1.0 / max(uGamma, 1e-3)));
    c = mix(vec3(dot(c, REC709_LUMA)), c, uSaturation);
    fragColor = TDOutputSwizzle(vec4(max(c, 0.0), 1.0));
}
