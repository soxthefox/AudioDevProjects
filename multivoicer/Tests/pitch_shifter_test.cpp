#define _USE_MATH_DEFINES
#include <cmath>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include "Dsp/SignalsmithPitchShifter.h"
#include "Dsp/YinPitchDetector.h"

namespace {
std::vector<float> sine(double freqHz, double sr, int n, float amp = 0.5f) {
    std::vector<float> out(n);
    for (int i = 0; i < n; ++i) out[i] = amp * std::sin(2.0 * M_PI * freqHz * i / sr);
    return out;
}
}

TEST_CASE("SignalsmithPitchShifter shifts pitch by ratio", "[shifter]") {
    const double sr = 44100.0;
    const int block = 1024;
    const int totalBlocks = 16;
    const int total = block * totalBlocks;

    mv::SignalsmithPitchShifter shifter;
    shifter.prepare(sr, block);

    mv::YinPitchDetector det;
    det.prepare(sr, block);

    for (double ratio : { 0.5, 0.75, 1.0, 1.25, 1.5, 2.0 }) {
        shifter.reset();
        det.reset();
        shifter.setShiftRatio(ratio);

        auto in = sine(440.0, sr, total);
        std::vector<float> out(total, 0.0f);
        for (int b = 0; b < totalBlocks; ++b) {
            shifter.process(in.data() + b * block, out.data() + b * block, block);
        }

        // Feed only the second half of the output to the detector
        // (skip the shifter's startup latency).
        auto r = det.process(out.data() + total / 2, total / 2);
        INFO("ratio = " << ratio);
        CHECK(r.voiced);
        CHECK(std::abs(r.freqHz - 440.0 * ratio) < 10.0);
    }
}

TEST_CASE("SignalsmithPitchShifter unity ratio is near-passthrough", "[shifter]") {
    const double sr = 44100.0;
    const int block = 1024;
    const int total = block * 8;

    mv::SignalsmithPitchShifter shifter;
    shifter.prepare(sr, block);
    shifter.setShiftRatio(1.0);

    auto in = sine(440.0, sr, total);
    std::vector<float> out(total, 0.0f);
    for (int b = 0; b < 8; ++b) {
        shifter.process(in.data() + b * block, out.data() + b * block, block);
    }

    // After startup, peak amplitude should be within ~3 dB of input (0.5 amp).
    float peak = 0.0f;
    for (int i = total / 2; i < total; ++i) peak = std::max(peak, std::abs(out[i]));
    CHECK(peak > 0.3f);
    CHECK(peak < 1.0f);
}
