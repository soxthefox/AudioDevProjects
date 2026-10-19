#include "YinPitchDetector.h"
#include <algorithm>
#include <cmath>

namespace mv {

void YinPitchDetector::prepare(double sr, int /*maxBlockSize*/) {
    sampleRate = sr;
    analysisFrameSize = 2048;
    minLag = std::max(2, static_cast<int>(sampleRate / fmaxHz));
    maxLag = std::min(analysisFrameSize / 2, static_cast<int>(sampleRate / fminHz));
    ringBuffer.assign(analysisFrameSize * 2, 0.0f);
    reset();
}

void YinPitchDetector::reset() {
    std::fill(ringBuffer.begin(), ringBuffer.end(), 0.0f);
    writePos = 0;
    samplesSinceLastEstimate = analysisFrameSize;  // force a fresh estimate on first call
    lastResult = {};
}

PitchDetector::Result YinPitchDetector::process(const float* mono, int n) {
    const int cap = static_cast<int>(ringBuffer.size());
    for (int i = 0; i < n; ++i) {
        ringBuffer[writePos] = mono[i];
        writePos = (writePos + 1) % cap;
    }
    samplesSinceLastEstimate += n;

    // One YIN analysis per analysisFrameSize hop (so detector latency ~ one hop).
    if (samplesSinceLastEstimate >= analysisFrameSize) {
        samplesSinceLastEstimate = 0;
        lastResult = runYin();
    }
    return lastResult;
}

PitchDetector::Result YinPitchDetector::runYin() {
    // Copy the most recent analysisFrameSize samples into a contiguous frame.
    const int N = analysisFrameSize;
    std::vector<float> frame(N);
    const int cap = static_cast<int>(ringBuffer.size());
    int idx = (writePos - N + cap) % cap;
    for (int i = 0; i < N; ++i) {
        frame[i] = ringBuffer[(idx + i) % cap];
    }

    // Step 1 — difference function d(tau)
    std::vector<float> d(maxLag + 1, 0.0f);
    for (int tau = 1; tau <= maxLag; ++tau) {
        float sum = 0.0f;
        for (int i = 0; i + tau < N; ++i) {
            float diff = frame[i] - frame[i + tau];
            sum += diff * diff;
        }
        d[tau] = sum;
    }

    // Step 2 — cumulative mean normalized difference (CMNDF)
    std::vector<float> cmnd(maxLag + 1, 1.0f);
    cmnd[0] = 1.0f;
    float running = 0.0f;
    for (int tau = 1; tau <= maxLag; ++tau) {
        running += d[tau];
        cmnd[tau] = d[tau] * tau / (running > 1e-12f ? running : 1e-12f);
    }

    // Step 3 — absolute threshold: first tau where cmnd < threshold AND is a local minimum
    int tauEstimate = -1;
    for (int tau = minLag; tau <= maxLag; ++tau) {
        if (cmnd[tau] < cmndfThreshold) {
            while (tau + 1 <= maxLag && cmnd[tau + 1] < cmnd[tau]) ++tau;
            tauEstimate = tau;
            break;
        }
    }

    Result r {};
    if (tauEstimate < 0) {
        r.voiced = false;
        r.confidence = 0.0f;
        return r;
    }

    // Step 4 — parabolic interpolation around tauEstimate for sub-sample accuracy
    double betterTau = tauEstimate;
    if (tauEstimate > 0 && tauEstimate < maxLag) {
        float s0 = cmnd[tauEstimate - 1];
        float s1 = cmnd[tauEstimate];
        float s2 = cmnd[tauEstimate + 1];
        float denom = (2.0f * (2.0f * s1 - s2 - s0));
        if (std::abs(denom) > 1e-12f) {
            betterTau = tauEstimate + (s2 - s0) / denom;
        }
    }

    r.freqHz = sampleRate / betterTau;
    r.confidence = std::clamp(1.0f - cmnd[tauEstimate], 0.0f, 1.0f);
    r.voiced = (r.freqHz >= fminHz && r.freqHz <= fmaxHz && r.confidence >= 0.5f);
    return r;
}

} // namespace mv
