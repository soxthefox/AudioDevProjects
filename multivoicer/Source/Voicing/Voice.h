#pragma once

#include <atomic>
#include <memory>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Dsp/PitchShifter.h"
#include "../Dsp/VoiceEq.h"
#include "../Dsp/Envelope.h"

namespace mv {

struct VoiceParams {
    bool   enabled = false;
    int    interval = 0;
    float  gainDb = 0.0f;
    float  pan = 0.0f;
    float  attack = 20.0f, attackLevel = 1.0f;
    float  decay = 80.0f, sustain = 0.8f;
    float  release = 300.0f, releaseLevel = 0.0f;
    float  lowShelfDb = 0.0f, midPeakDb = 0.0f, highShelfDb = 0.0f;
    float  lowShelfHz = 200.0f, midFreqHz = 1000.0f, midQ = 1.0f, highShelfHz = 4000.0f;
};

class Voice {
public:
    Voice();
    ~Voice();

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setParams(const VoiceParams& p);
    void setTargetFreq(double hz);
    void noteOn();
    void noteOff();
    bool isGated() const { return envelope.isActive(); }
    void renderAdd(const float* dryMono, float* outL, float* outR, int n, double inputFreqHz);

    // Peak output level from the last renderAdd call, 0 when idle — read by
    // the UI (waveform visualiser) from the message thread; safe to poll.
    float level() const { return currentLevel.load(std::memory_order_relaxed); }

private:
    std::unique_ptr<PitchShifter> shifter;
    VoiceEq eq;
    Envelope envelope;
    Envelope::Parameters envelopeParams;
    double sampleRate = 44100.0;
    double targetFreq = 440.0;
    float  gainLinear = 1.0f;
    float  panLeft = 0.707f, panRight = 0.707f;
    std::vector<float> tmp;
    std::atomic<float> currentLevel { 0.0f };
};

} // namespace mv
