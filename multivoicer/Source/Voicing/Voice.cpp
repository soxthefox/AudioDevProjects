#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include "Voice.h"
#include "../Dsp/SignalsmithPitchShifter.h"

namespace mv {

Voice::Voice() : shifter(std::make_unique<SignalsmithPitchShifter>()) {}
Voice::~Voice() = default;

void Voice::prepare(double sr, int maxBlockSize) {
    sampleRate = sr;
    shifter->prepare(sr, maxBlockSize);
    eq.prepare(sr, maxBlockSize);
    envelope.setSampleRate(sr);
    tmp.assign((size_t)maxBlockSize, 0.0f);
    reset();
}

void Voice::reset() {
    shifter->reset();
    eq.reset();
    envelope.reset();
}

void Voice::setParams(const VoiceParams& p) {
    envelopeParams.attackMs = p.attack;
    envelopeParams.attackLevel = p.attackLevel;
    envelopeParams.decayMs = p.decay;
    envelopeParams.sustainLevel = p.sustain;
    envelopeParams.releaseMs = p.release;
    envelopeParams.releaseLevel = p.releaseLevel;
    envelope.setParameters(envelopeParams);
    eq.setParams(p.lowShelfDb, p.lowShelfHz, p.midPeakDb, p.midFreqHz, p.midQ, p.highShelfDb, p.highShelfHz);
    gainLinear = std::pow(10.0f, p.gainDb / 20.0f);
    // Equal-power pan (-1..+1)
    float panNorm = (p.pan + 1.0f) * 0.5f * (float)M_PI_2;
    panLeft  = std::cos(panNorm);
    panRight = std::sin(panNorm);
}

void Voice::setTargetFreq(double hz) {
    targetFreq = hz > 0.0 ? hz : targetFreq;
}

void Voice::noteOn()  { envelope.noteOn(); }
void Voice::noteOff() { envelope.noteOff(); }

void Voice::renderAdd(const float* dryMono, float* outL, float* outR, int n, double inputFreqHz) {
    if (!envelope.isActive()) {
        currentLevel.store(0.0f, std::memory_order_relaxed);
        return;
    }

    const double safeInput = inputFreqHz > 1.0 ? inputFreqHz : 100.0;
    shifter->setShiftRatio(targetFreq / safeInput);
    shifter->process(dryMono, tmp.data(), n);
    eq.processInPlace(tmp.data(), n);

    float peak = 0.0f;
    for (int i = 0; i < n; ++i) {
        float env = envelope.getNextSample();
        float s = tmp[i] * env * gainLinear;
        outL[i] += s * panLeft;
        outR[i] += s * panRight;
        peak = std::max(peak, std::abs(s));
    }
    currentLevel.store(peak, std::memory_order_relaxed);
}

} // namespace mv
