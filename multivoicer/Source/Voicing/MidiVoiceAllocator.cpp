#include "MidiVoiceAllocator.h"
#include <algorithm>

namespace mv {

void MidiVoiceAllocator::setPoly(int n) {
    mono = false;
    polyCount = std::clamp(n, 1, kMaxSlots);
    clearAll();
}

void MidiVoiceAllocator::setMono() {
    mono = true;
    polyCount = 1;
    clearAll();
}

void MidiVoiceAllocator::clearAll() {
    for (auto& s : slots) s = {};
    monoStack.clear();
}

void MidiVoiceAllocator::noteOn(int n) {
    if (mono) {
        auto it = std::find(monoStack.begin(), monoStack.end(), n);
        if (it != monoStack.end()) monoStack.erase(it);
        monoStack.push_back(n);
        slots[0] = { true, n, 0 };
        return;
    }
    // Poly: free slot first, else steal oldest active among 0..polyCount-1.
    for (int i = 0; i < polyCount; ++i) {
        if (!slots[i].active) {
            slots[i] = { true, n, 0 };
            return;
        }
    }
    int oldest = 0;
    for (int i = 1; i < polyCount; ++i)
        if (slots[i].age > slots[oldest].age) oldest = i;
    slots[oldest] = { true, n, 0 };
}

void MidiVoiceAllocator::noteOff(int n) {
    if (mono) {
        auto it = std::find(monoStack.begin(), monoStack.end(), n);
        if (it != monoStack.end()) monoStack.erase(it);
        if (monoStack.empty()) slots[0] = {};
        else                   slots[0] = { true, monoStack.back(), 0 };
        return;
    }
    for (int i = 0; i < polyCount; ++i) {
        if (slots[i].active && slots[i].midiNote == n) {
            slots[i] = {};
            return;
        }
    }
    // Not found (already stolen) — ignore.
}

void MidiVoiceAllocator::tick() {
    for (auto& s : slots) if (s.active) ++s.age;
}

} // namespace mv
