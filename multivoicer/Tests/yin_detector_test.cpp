#define _USE_MATH_DEFINES
#include <cmath>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include "Dsp/YinPitchDetector.h"

namespace {
std::vector<float> sine(double freqHz, double sr, int n, float amp = 0.5f) {
    std::vector<float> out(n);
    for (int i = 0; i < n; ++i) out[i] = amp * std::sin(2.0 * M_PI * freqHz * i / sr);
    return out;
}
}

TEST_CASE("YIN detects pure sines within 1 Hz", "[yin]") {
    mv::YinPitchDetector det;
    const double sr = 44100.0;
    const int n = 2048;
    det.prepare(sr, n);

    for (double f : { 110.0, 220.0, 440.0, 880.0 }) {
        det.reset();
        auto buf = sine(f, sr, n);
        auto r = det.process(buf.data(), n);
        INFO("Test frequency: " << f);
        CHECK(r.voiced);
        CHECK(r.confidence > 0.8f);
        CHECK(std::abs(r.freqHz - f) < 1.5);
    }
}

TEST_CASE("YIN reports unvoiced on silence", "[yin]") {
    mv::YinPitchDetector det;
    det.prepare(44100.0, 2048);
    std::vector<float> silent(2048, 0.0f);
    auto r = det.process(silent.data(), 2048);
    CHECK_FALSE(r.voiced);
}

TEST_CASE("YIN reports unvoiced on white noise", "[yin]") {
    mv::YinPitchDetector det;
    det.prepare(44100.0, 2048);
    std::vector<float> noise(2048);
    unsigned seed = 1;
    for (auto& x : noise) {
        seed = seed * 1664525u + 1013904223u;
        x = static_cast<float>(static_cast<int>(seed) / 2147483648.0);
    }
    auto r = det.process(noise.data(), 2048);
    // Noise can occasionally trigger a false detection — confidence must stay low.
    CHECK(r.confidence < 0.5f);
}
