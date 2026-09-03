#include "VoiceEq.h"
#include <cmath>

namespace mv {

static float dbToGain(float dB) { return std::pow(10.0f, dB / 20.0f); }

void VoiceEq::prepare(double sampleRate, int maxBlockSize) {
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32)maxBlockSize, 1 };
    lowShelf.prepare(spec);
    midPeak.prepare(spec);
    highShelf.prepare(spec);
    setParams(0.0f, 200.0f, 0.0f, 1000.0f, 1.0f, 0.0f, 4000.0f);
}

void VoiceEq::reset() {
    lowShelf.reset();
    midPeak.reset();
    highShelf.reset();
}

void VoiceEq::setParams(float lowDb, float lowHz, float midDb, float midHz, float midQ,
                         float highDb, float highHz) {
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    *lowShelf.coefficients  = *Coeffs::makeLowShelf(sr,  lowHz, 0.707f, dbToGain(lowDb));
    *midPeak.coefficients   = *Coeffs::makePeakFilter(sr, midHz, midQ, dbToGain(midDb));
    *highShelf.coefficients = *Coeffs::makeHighShelf(sr, highHz, 0.707f, dbToGain(highDb));
}

void VoiceEq::processInPlace(float* mono, int n) {
    juce::dsp::AudioBlock<float> block(&mono, 1, (size_t)n);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    lowShelf.process(ctx);
    midPeak.process(ctx);
    highShelf.process(ctx);
}

} // namespace mv
