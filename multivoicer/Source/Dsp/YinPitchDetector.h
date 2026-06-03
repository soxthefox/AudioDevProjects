#pragma once

#include <vector>
#include "PitchDetector.h"

namespace mv {

class YinPitchDetector : public PitchDetector {
public:
    void   prepare(double sampleRate, int maxBlockSize) override;
    void   reset() override;
    Result process(const float* mono, int numSamples) override;

private:
    double sampleRate = 44100.0;
    int    analysisFrameSize = 2048;     // window for one detection
    int    minLag = 0;                   // ~ sr / fmaxHz
    int    maxLag = 0;                   // ~ sr / fminHz
    static constexpr double fminHz = 60.0;
    static constexpr double fmaxHz = 2000.0;
    static constexpr float  cmndfThreshold = 0.15f;

    std::vector<float> ringBuffer;
    int    writePos = 0;
    int    samplesSinceLastEstimate = 0;
    Result lastResult {};

    Result runYin();   // operates on the most recent analysisFrameSize samples of ringBuffer
};

} // namespace mv
