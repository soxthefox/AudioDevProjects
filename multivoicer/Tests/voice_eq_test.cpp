#define _USE_MATH_DEFINES
#include <cmath>
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include "Dsp/VoiceEq.h"

namespace {
std::vector<float> sine(double freqHz, double sr, int n, float amp = 0.5f) {
    std::vector<float> out(n);
    for (int i = 0; i < n; ++i) out[i] = amp * std::sin(2.0 * M_PI * freqHz * i / sr);
    return out;
}
float peak(const std::vector<float>& v, int from) {
    float p = 0.0f;
    for (int i = from; i < (int)v.size(); ++i) p = std::max(p, std::abs(v[i]));
    return p;
}
float toDb(float linear) { return 20.0f * std::log10(linear + 1e-12f); }
}

TEST_CASE("VoiceEq flat at 0 dB gains", "[voice_eq]") {
    mv::VoiceEq eq;
    eq.prepare(44100.0, 1024);
    eq.setParams(0.0f, 0.0f, 1000.0f, 1.0f, 0.0f);
    auto in = sine(1000.0, 44100.0, 4096);
    std::vector<float> out = in;
    eq.processInPlace(out.data(), (int)out.size());
    float inPeak  = peak(in, 1024);
    float outPeak = peak(out, 1024);
    CHECK(std::abs(toDb(outPeak) - toDb(inPeak)) < 0.5f);  // within 0.5 dB
}

TEST_CASE("VoiceEq mid peak boosts at its center frequency", "[voice_eq]") {
    mv::VoiceEq eq;
    eq.prepare(44100.0, 1024);
    eq.setParams(0.0f, +12.0f, 1000.0f, 1.0f, 0.0f);
    auto in = sine(1000.0, 44100.0, 8192);
    std::vector<float> out = in;
    eq.processInPlace(out.data(), (int)out.size());
    float inPeak  = peak(in, 2048);
    float outPeak = peak(out, 2048);
    // +12 dB target. Allow ±2 dB headroom.
    float gainDb = toDb(outPeak) - toDb(inPeak);
    CHECK(gainDb > 10.0f);
    CHECK(gainDb < 14.0f);
}

TEST_CASE("VoiceEq low shelf cuts below shelf frequency", "[voice_eq]") {
    mv::VoiceEq eq;
    eq.prepare(44100.0, 1024);
    eq.setParams(-12.0f, 0.0f, 1000.0f, 1.0f, 0.0f);  // -12 dB low shelf
    auto in = sine(80.0, 44100.0, 8192);
    std::vector<float> out = in;
    eq.processInPlace(out.data(), (int)out.size());
    float inPeak  = peak(in, 2048);
    float outPeak = peak(out, 2048);
    float gainDb = toDb(outPeak) - toDb(inPeak);
    CHECK(gainDb < -8.0f);   // significantly attenuated
}
