#include "MusicTheory.h"
#include <cmath>

namespace mv {

int midiFromFreq(double hz) {
    if (hz <= 0.0) return 0;
    return static_cast<int>(std::lround(69.0 + 12.0 * std::log2(hz / 440.0)));
}

double freqFromMidi(double midi) {
    return 440.0 * std::pow(2.0, (midi - 69.0) / 12.0);
}

// Stubs for later tasks — keep linker happy until Tasks 9/10/11 implement them.
int quantizeToKey(int, Key)               { return 0; }
int scaleDegreeOf(int, Key)               { return -1; }
int targetForInterval(int, int, Key)      { return 0; }

} // namespace mv
