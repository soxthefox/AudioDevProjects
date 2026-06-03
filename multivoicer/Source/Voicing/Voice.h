#pragma once

#include <memory>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Dsp/PitchShifter.h"
#include "../Dsp/VoiceEq.h"

namespace mv {

struct VoiceParams {
    bool   enabled = false;
    int    interval = 0;
    float  gainDb = 0.0f;
    float  pan = 0.0f;
    float  attack = 20.0f, decay = 80.0f, sustain = 0.8f, release = 300.0f;
    float  lowShelfDb = 0.0f, midPeakDb = 0.0f, highShelfDb = 0.0f;
    float  midFreqHz = 1000.0f, midQ = 1.0f;
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
    bool isGated() const { return adsr.isActive(); }
    void renderAdd(const float* dryMono, float* outL, float* outR, int n, double inputFreqHz);

private:
    std::unique_ptr<PitchShifter> shifter;
    VoiceEq eq;
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;
    double sampleRate = 44100.0;
    double targetFreq = 440.0;
    float  gainLinear = 1.0f;
    float  panLeft = 0.707f, panRight = 0.707f;
    std::vector<float> tmp;
};

} // namespace mv
