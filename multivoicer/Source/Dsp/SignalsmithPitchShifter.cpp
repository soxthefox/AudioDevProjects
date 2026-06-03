#include "SignalsmithPitchShifter.h"
#include "signalsmith-stretch/signalsmith-stretch.h"

namespace mv {

struct SignalsmithPitchShifter::Impl {
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    double sampleRate = 44100.0;
    double ratio = 1.0;
    bool   formantPreserve = true;
    bool   prepared = false;

    void configure() {
        stretch.presetDefault(1, static_cast<float>(sampleRate));
        applyShift();
    }

    void applyShift() {
        // setTransposeFactor: first arg is pitch multiplier, second is tonality limit (0 = no limit).
        stretch.setTransposeFactor(static_cast<float>(ratio), 0.0f);

        // Formant preservation: setFormantFactor(1/ratio) keeps formants at original frequency.
        // When off, use factor 1.0 (formants follow the pitch shift).
        if (formantPreserve) {
            stretch.setFormantFactor(static_cast<float>(1.0 / ratio));
        } else {
            stretch.setFormantFactor(1.0f);
        }
    }
};

SignalsmithPitchShifter::SignalsmithPitchShifter()
    : impl(std::make_unique<Impl>()) {}

SignalsmithPitchShifter::~SignalsmithPitchShifter() = default;

void SignalsmithPitchShifter::prepare(double sr, int /*maxBlockSize*/) {
    impl->sampleRate = sr;
    impl->configure();
    impl->prepared = true;
}

void SignalsmithPitchShifter::reset() {
    if (impl->prepared) impl->stretch.reset();
}

void SignalsmithPitchShifter::setShiftRatio(double r) {
    impl->ratio = (r > 0.0) ? r : 1.0;
    if (impl->prepared) impl->applyShift();
}

void SignalsmithPitchShifter::setFormantPreserve(bool on) {
    impl->formantPreserve = on;
    if (impl->prepared) impl->applyShift();
}

void SignalsmithPitchShifter::process(const float* in, float* out, int n) {
    const float* inputs[1]  = { in };
    float*       outputs[1] = { out };
    impl->stretch.process(inputs, n, outputs, n);
}

} // namespace mv
