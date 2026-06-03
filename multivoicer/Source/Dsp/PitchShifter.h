#pragma once

namespace mv {

class PitchShifter {
public:
    virtual ~PitchShifter() = default;

    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;
    virtual void setShiftRatio(double outputFreqOverInputFreq) = 0;
    virtual void setFormantPreserve(bool on) = 0;
    virtual void process(const float* in, float* out, int numSamples) = 0;
};

} // namespace mv
