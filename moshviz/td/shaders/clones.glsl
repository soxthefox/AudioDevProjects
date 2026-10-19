// moshviz clones pass (optional): copies of the performer beside them.
//
// GLSL TOP setup (two wired inputs):
//   input 0: look (the finished image)
//   input 1: mask (subject mask, white = performer)
//   Pixel Format: 16-bit float (RGBA).
// Sits between `look` and `out`. To unplug it, bypass the node (Textport:
// op('/project1/clones').bypass = True) or set uCloneCount to 0. A bypassed
// TOP passes input 0 through unchanged, so nothing else needs rewiring.
// Meant for a single performer; with a real crowd, unplug it.
//
// Clone k (1, 2, 3, ...) sits on alternating sides: right, left, right, ...
// Its rank (distance from the performer) is ceil(k / 2). A clone of rank n
// is shifted n * uCloneSpacing across the frame and scaled by
// uCloneScale^n around the bottom edge, so smaller clones read as further
// back while still standing on the same floor line. Drawing goes from the
// furthest rank inwards, with the performer on top, so nearer figures cover
// further ones.

uniform float uCloneCount;    // number of clones, 0 to MAX_CLONES (0 = none)
uniform float uCloneSpacing;  // horizontal step between ranks, as a fraction of frame width (e.g. 0.28)
uniform float uCloneScale;    // size of each rank relative to the previous one (e.g. 0.85)
uniform float uCloneFade;     // brightness of each rank relative to the previous one (e.g. 0.7)
uniform float uMaskFeather;   // softness of the cut-out edge, in mask units (e.g. 0.1)

const int IMAGE = 0;
const int MASK = 1;
const int MAX_CLONES = 8;
const vec2 SCALE_PIVOT = vec2(0.5, 0.0);   // bottom centre: clones share the floor line

out vec4 fragColor;

float subjectAt(vec2 uv)
{
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
        return 0.0;
    }
    float m = texture(sTD2DInputs[MASK], uv).r;
    return smoothstep(0.5 - uMaskFeather, 0.5 + uMaskFeather, m);
}

// Where in the original frame this pixel's clone of `rank` on `side`
// (+1 right, -1 left) samples from.
vec2 cloneSourceUV(vec2 uv, float rank, float side)
{
    float scale = pow(uCloneScale, rank);
    vec2 offset = vec2(side * rank * uCloneSpacing, 0.0);
    return SCALE_PIVOT + (uv - SCALE_PIVOT - offset) / max(scale, 1e-3);
}

void main()
{
    vec2 uv = vUV.st;
    vec4 base = texture(sTD2DInputs[IMAGE], uv);
    vec3 colour = base.rgb;

    int count = clamp(int(uCloneCount + 0.5), 0, MAX_CLONES);
    int maxRank = (count + 1) / 2;
    for (int rank = maxRank; rank >= 1; --rank) {
        // Clone 2r-1 is on the right, clone 2r on the left.
        for (int side = 0; side < 2; ++side) {
            int k = 2 * rank - 1 + side;
            if (k > count) {
                continue;
            }
            float sideSign = side == 0 ? 1.0 : -1.0;
            vec2 src = cloneSourceUV(uv, float(rank), sideSign);
            float coverage = subjectAt(src);
            vec3 figure = texture(sTD2DInputs[IMAGE], src).rgb * pow(uCloneFade, float(rank));
            colour = mix(colour, figure, coverage);
        }
    }

    // The performer stays in front of every clone.
    colour = mix(colour, base.rgb, subjectAt(uv));
    fragColor = TDOutputSwizzle(vec4(colour, 1.0));
}
