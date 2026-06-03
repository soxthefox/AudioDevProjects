#define _USE_MATH_DEFINES
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
    adsr.setSampleRate(sr);
    tmp.assign((size_t)maxBlockSize, 0.0f);
    reset();
}

void Voice::reset() {
    shifter->reset();
    eq.reset();
    adsr.reset();
}

void Voice::setParams(const VoiceParams& p) {
    adsrParams.attack  = p.attack  * 0.001f;
    adsrParams.decay   = p.decay   * 0.001f;
    adsrParams.sustain = p.sustain;
    adsrParams.release = p.release * 0.001f;
    adsr.setParameters(adsrParams);
    eq.setParams(p.lowShelfDb, p.midPeakDb, p.midFreqHz, p.midQ, p.highShelfDb);
    gainLinear = std::pow(10.0f, p.gainDb / 20.0f);
    // Equal-power pan (-1..+1)
    float panNorm = (p.pan + 1.0f) * 0.5f * (float)M_PI_2;
    panLeft  = std::cos(panNorm);
    panRight = std::sin(panNorm);
}

void Voice::setTargetFreq(double hz) {
    targetFreq = hz > 0.0 ? hz : targetFreq;
}

void Voice::noteOn()  { adsr.noteOn(); }
void Voice::noteOff() { adsr.noteOff(); }

void Voice::renderAdd(const float* dryMono, float* outL, float* outR, int n, double inputFreqHz) {
    if (!adsr.isActive()) return;

    const double safeInput = inputFreqHz > 1.0 ? inputFreqHz : 100.0;
    shifter->setShiftRatio(targetFreq / safeInput);
    shifter->process(dryMono, tmp.data(), n);
    eq.processInPlace(tmp.data(), n);

    for (int i = 0; i < n; ++i) {
        float env = adsr.getNextSample();
        float s = tmp[i] * env * gainLinear;
        outL[i] += s * panLeft;
        outR[i] += s * panRight;
    }
}

} // namespace mv
