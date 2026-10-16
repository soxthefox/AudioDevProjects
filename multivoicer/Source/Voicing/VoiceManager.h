#pragma once

#include <array>
#include "Voice.h"

namespace mv {

class VoiceManager {
public:
    static constexpr int kMaxVoices = 4;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setNumActiveVoices(int n);
    int  numActiveVoices() const { return active; }
    Voice& voice(int i) { return voices[i]; }
    void renderAdd(const float* dry, float* outL, float* outR, int n, double inputFreqHz);

private:
    std::array<Voice, kMaxVoices> voices;
    int active = 3;
};

} // namespace mv
