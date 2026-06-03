#pragma once

#include <juce_dsp/juce_dsp.h>

namespace mv {

class VoiceEq {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setParams(float lowShelfDb, float midPeakDb, float midFreqHz, float midQ, float highShelfDb);
    void processInPlace(float* mono, int numSamples);

private:
    double sr = 44100.0;
    juce::dsp::IIR::Filter<float> lowShelf;
    juce::dsp::IIR::Filter<float> midPeak;
    juce::dsp::IIR::Filter<float> highShelf;
};

} // namespace mv
