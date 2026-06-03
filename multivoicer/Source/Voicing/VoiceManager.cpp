#include "VoiceManager.h"
#include <algorithm>

namespace mv {

void VoiceManager::prepare(double sr, int maxBlockSize) {
    for (auto& v : voices) v.prepare(sr, maxBlockSize);
}

void VoiceManager::reset() {
    for (auto& v : voices) v.reset();
}

void VoiceManager::setNumActiveVoices(int n) {
    active = std::clamp(n, 1, kMaxVoices);
    for (int i = active; i < kMaxVoices; ++i) voices[i].noteOff();
}

void VoiceManager::renderAdd(const float* dry, float* outL, float* outR, int n, double inputFreqHz) {
    for (int i = 0; i < active; ++i) {
        voices[i].renderAdd(dry, outL, outR, n, inputFreqHz);
    }
}

} // namespace mv
