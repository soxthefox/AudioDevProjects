#pragma once

#include <juce_core/juce_core.h>
#include "../../Music/MusicTheory.h"

// Small display-string helpers so panels don't duplicate note-naming logic.
// Builds on the same mv::MusicTheory functions the DSP uses, so a voice
// card's readout always agrees with what the harmonizer actually targets.
namespace mv::ui::Labels {

inline juce::String noteName(int pitchClass) {
    static const char* names[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    return names[((pitchClass % 12) + 12) % 12];
}

inline juce::String midiNoteName(int midiNote) {
    if (midiNote < 0) return "--";
    return noteName(midiNote % 12) + juce::String(midiNote / 12 - 1);
}

inline juce::String intervalQuality(int semitoneDistance) {
    static const char* names[13] = {
        "UNISON","MIN 2ND","MAJ 2ND","MIN 3RD","MAJ 3RD","4TH","TRITONE",
        "5TH","MIN 6TH","MAJ 6TH","MIN 7TH","MAJ 7TH","OCTAVE"
    };
    int d = std::abs(semitoneDistance) % 12;
    if (std::abs(semitoneDistance) > 0 && d == 0) d = 12;
    return names[d];
}

// Returns { target note letter, interval quality } for a diatonic interval
// applied from the key's root — what a VoiceCard shows in Preset mode.
inline std::pair<juce::String, juce::String> intervalTarget(mv::Key key, int diatonicInterval) {
    const int rootMidi = 60 + (int) key.root;
    const int target = mv::targetForInterval(rootMidi, diatonicInterval, key);
    return { noteName(target), intervalQuality(target - rootMidi) };
}

} // namespace mv::ui::Labels
