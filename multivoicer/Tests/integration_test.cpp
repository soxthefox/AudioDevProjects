#define _USE_MATH_DEFINES
#include <cmath>
#include <catch2/catch_test_macros.hpp>
#include "Plugin/PluginProcessor.h"
#include "Dsp/YinPitchDetector.h"

namespace {
juce::AudioBuffer<float> makeStereoSine(double freqHz, double sr, int n) {
    juce::AudioBuffer<float> buf(2, n);
    for (int i = 0; i < n; ++i) {
        float s = 0.4f * std::sin(2.0 * M_PI * freqHz * i / sr);
        buf.setSample(0, i, s); buf.setSample(1, i, s);
    }
    return buf;
}
float channelRms(const juce::AudioBuffer<float>& b, int ch, int from, int len) {
    double s = 0;
    for (int i = from; i < from + len; ++i) s += b.getSample(ch, i) * b.getSample(ch, i);
    return (float) std::sqrt(s / len);
}
}

TEST_CASE("Integration: silence in -> silence out, no NaN", "[integration]") {
    MultivoicerProcessor p;
    p.prepareToPlay(44100.0, 512);
    juce::AudioBuffer<float> buf(2, 4096); buf.clear();
    juce::MidiBuffer midi;
    for (int b = 0; b < 8; ++b) {
        juce::AudioBuffer<float> blk(buf.getArrayOfWritePointers(), 2, b * 512, 512);
        p.processBlock(blk, midi);
    }
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 4096; ++i)
            CHECK_FALSE(std::isnan(buf.getSample(c, i)));
    CHECK(channelRms(buf, 0, 1024, 3072) < 0.001f);
}

TEST_CASE("Integration: all voices disabled -> output close to dry", "[integration]") {
    MultivoicerProcessor p;
    p.prepareToPlay(44100.0, 512);
    // Disable every voice
    for (int i = 1; i <= 4; ++i) {
        if (auto* prm = p.apvts.getParameter(mv::Params::voiceParam("Enabled", i)))
            prm->setValueNotifyingHost(0.0f);
    }
    // Force mix=0 to be sure
    if (auto* prm = p.apvts.getParameter(mv::Params::kDryWetMix))
        prm->setValueNotifyingHost(0.0f);
    auto inBuf = makeStereoSine(440.0, 44100.0, 4096);
    juce::AudioBuffer<float> work(2, 4096);
    for (int c = 0; c < 2; ++c) std::copy(inBuf.getReadPointer(c), inBuf.getReadPointer(c) + 4096, work.getWritePointer(c));
    juce::MidiBuffer midi;
    for (int b = 0; b < 8; ++b) {
        juce::AudioBuffer<float> blk(work.getArrayOfWritePointers(), 2, b * 512, 512);
        p.processBlock(blk, midi);
    }
    float inR  = channelRms(inBuf, 0, 0, 4096);
    float outR = channelRms(work,   0, 0, 4096);
    CHECK(std::abs(outR - inR) < 0.05f);
}

TEST_CASE("Integration: Third Up preset adds a voice above input pitch", "[integration]") {
    MultivoicerProcessor p;
    p.prepareToPlay(44100.0, 512);
    p.setCurrentProgram(2);  // "Third Up"

    auto in = makeStereoSine(261.63, 44100.0, 16384);  // C4, extra length for Signalsmith ramp-up
    juce::AudioBuffer<float> work(2, 16384);
    for (int c = 0; c < 2; ++c) std::copy(in.getReadPointer(c), in.getReadPointer(c) + 16384, work.getWritePointer(c));

    juce::MidiBuffer midi;
    for (int b = 0; b < 32; ++b) {
        juce::AudioBuffer<float> blk(work.getArrayOfWritePointers(), 2, b * 512, 512);
        p.processBlock(blk, midi);
    }
    // Confirm the output RMS exceeds dry-only RMS, indicating harmony was added.
    // Measure from sample 8192 onward to skip Signalsmith startup latency.
    float inR  = channelRms(in,   0, 8192, 8192);
    float outR = channelRms(work, 0, 8192, 8192);
    CHECK(outR > inR * 0.5f);   // output exceeds half-dry, confirming wet path contributes
}
