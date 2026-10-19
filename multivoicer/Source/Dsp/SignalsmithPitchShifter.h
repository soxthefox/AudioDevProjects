#pragma once

#include <memory>
#include "PitchShifter.h"

namespace mv {

class SignalsmithPitchShifter : public PitchShifter {
public:
    SignalsmithPitchShifter();
    ~SignalsmithPitchShifter() override;

    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void setShiftRatio(double ratio) override;
    void setFormantPreserve(bool on) override;
    void process(const float* in, float* out, int numSamples) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace mv
