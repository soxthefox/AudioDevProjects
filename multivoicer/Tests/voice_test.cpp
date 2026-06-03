#define _USE_MATH_DEFINES
#include <cmath>
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include "Voicing/Voice.h"

namespace {
std::vector<float> sine(double freqHz, double sr, int n, float amp = 0.5f) {
    std::vector<float> out(n);
    for (int i = 0; i < n; ++i) out[i] = amp * std::sin(2.0 * M_PI * freqHz * i / sr);
    return out;
}
float rms(const float* p, int n) {
    double s = 0; for (int i = 0; i < n; ++i) s += p[i] * p[i];
    return (float)std::sqrt(s / n);
}
}

TEST_CASE("Voice silent before noteOn", "[voice]") {
    mv::Voice v;
    v.prepare(44100.0, 512);
    mv::VoiceParams p {};
    p.enabled = true; p.gainDb = 0.0f; p.pan = 0.0f;
    p.attack = 10.0f; p.decay = 50.0f; p.sustain = 1.0f; p.release = 100.0f;
    v.setParams(p);
    v.setTargetFreq(440.0);
    auto in = sine(440.0, 44100.0, 512);
    std::vector<float> outL(512, 0.0f), outR(512, 0.0f);
    v.renderAdd(in.data(), outL.data(), outR.data(), 512, 440.0);
    CHECK(rms(outL.data(), 512) < 0.01f);  // no note-on => silent
}

TEST_CASE("Voice produces output after noteOn", "[voice]") {
    mv::Voice v;
    const int block = 1024;
    const int totalBlocks = 16;
    const int total = block * totalBlocks;
    v.prepare(44100.0, block);
    mv::VoiceParams p {};
    p.enabled = true; p.gainDb = 0.0f; p.pan = 0.0f;
    p.attack = 1.0f; p.decay = 10.0f; p.sustain = 1.0f; p.release = 100.0f;
    v.setParams(p);
    v.setTargetFreq(440.0);
    v.noteOn();

    auto in = sine(440.0, 44100.0, total);
    std::vector<float> outL(total, 0.0f), outR(total, 0.0f);
    for (int b = 0; b < totalBlocks; ++b) {
        v.renderAdd(in.data() + b * block, outL.data() + b * block, outR.data() + b * block, block, 440.0);
    }
    // Skip first half to clear shifter startup latency
    CHECK(rms(outL.data() + total / 2, total / 2) > 0.05f);
}

TEST_CASE("Voice pan routes to expected channel", "[voice]") {
    mv::Voice v;
    const int block = 1024;
    const int totalBlocks = 16;
    const int total = block * totalBlocks;
    v.prepare(44100.0, block);
    mv::VoiceParams p {};
    p.enabled = true; p.gainDb = 0.0f; p.pan = -1.0f;       // hard left
    p.attack = 1.0f; p.decay = 10.0f; p.sustain = 1.0f; p.release = 100.0f;
    v.setParams(p);
    v.setTargetFreq(440.0);
    v.noteOn();

    auto in = sine(440.0, 44100.0, total);
    std::vector<float> outL(total, 0.0f), outR(total, 0.0f);
    for (int b = 0; b < totalBlocks; ++b) {
        v.renderAdd(in.data() + b * block, outL.data() + b * block, outR.data() + b * block, block, 440.0);
    }
    // Skip first half to clear shifter startup latency
    CHECK(rms(outL.data() + total / 2, total / 2) > 0.05f);
    CHECK(rms(outR.data() + total / 2, total / 2) < 0.005f);
}
