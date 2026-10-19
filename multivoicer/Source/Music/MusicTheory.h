#pragma once

#include <array>
#include <cstdint>

namespace mv {

enum class NoteName : int {
    C = 0, Cs = 1, D = 2, Ds = 3, E = 4, F = 5,
    Fs = 6, G = 7, Gs = 8, A = 9, As = 10, B = 11
};

struct PitchClassSet {
    uint16_t mask;  // 12 LSBs used; bit i set iff semitone i above root is in the scale
    constexpr bool contains(int semitone) const {
        return ((mask >> (((semitone % 12) + 12) % 12)) & 1u) != 0u;
    }
};

struct ScaleDef {
    const char* name;
    PitchClassSet intervals;
};

struct Key {
    NoteName root;
    const ScaleDef* scale;
};

// Pure functions — defined in MusicTheory.cpp (Tasks 8–11).
int    midiFromFreq(double hz);
double freqFromMidi(double midi);
int    quantizeToKey(int midiNote, Key key);
int    scaleDegreeOf(int midiNote, Key key);   // 0=root, 1=2nd, ..., -1 if not in key
int    targetForInterval(int inputMidi, int diatonicInterval, Key key);

} // namespace mv
