#pragma once

namespace mv {

class PitchDetector {
public:
    struct Result {
        double freqHz = 0.0;
        float  confidence = 0.0f;
        bool   voiced = false;
    };

    virtual ~PitchDetector() = default;

    virtual void   prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void   reset() = 0;
    virtual Result process(const float* mono, int numSamples) = 0;
};

} // namespace mv
