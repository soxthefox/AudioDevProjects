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
int quantizeToKey(int midiNote, Key key) {
    // Already in key? Done.
    if (scaleDegreeOf(midiNote, key) >= 0) return midiNote;

    // Search outward (1, -1, 2, -2, ...). On tie at same |distance|,
    // the lower note wins because we test -distance before +distance.
    for (int distance = 1; distance <= 6; ++distance) {
        int down = midiNote - distance;
        if (scaleDegreeOf(down, key) >= 0) return down;
        int up = midiNote + distance;
        if (scaleDegreeOf(up, key) >= 0) return up;
    }
    return midiNote; // unreachable — every 12-semitone window has scale notes
}
int scaleDegreeOf(int midiNote, Key key) {
    int semitoneFromRoot = ((midiNote - static_cast<int>(key.root)) % 12 + 12) % 12;
    if (!key.scale->intervals.contains(semitoneFromRoot)) return -1;

    int degree = 0;
    for (int i = 0; i < semitoneFromRoot; ++i) {
        if (key.scale->intervals.contains(i)) ++degree;
    }
    return degree;
}
int targetForInterval(int, int, Key)      { return 0; }

} // namespace mv
