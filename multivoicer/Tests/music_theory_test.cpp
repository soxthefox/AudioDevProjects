#include <catch2/catch_test_macros.hpp>
#include "Music/MusicTheory.h"
#include "Music/Scales.h"

using namespace mv;

TEST_CASE("PitchClassSet::contains", "[music_theory]") {
    PitchClassSet major { 0b0000'1010'1101'0101 };
    CHECK(major.contains(0));   // root
    CHECK(major.contains(4));   // major 3rd
    CHECK(major.contains(7));   // 5th
    CHECK_FALSE(major.contains(1));  // minor 2nd not in major
    CHECK_FALSE(major.contains(3));  // minor 3rd not in major
    CHECK(major.contains(12));  // wraps: octave is in
    CHECK(major.contains(16));  // wraps: 4+12 == major 3rd of next octave
}

TEST_CASE("Scales registry has Major and NaturalMinor", "[music_theory]") {
    REQUIRE(Scales::All.size() == 2);
    bool foundMajor = false;
    bool foundMinor = false;
    for (const auto* s : Scales::All) {
        if (std::string_view(s->name) == "Major")         foundMajor = true;
        if (std::string_view(s->name) == "Natural Minor") foundMinor = true;
    }
    CHECK(foundMajor);
    CHECK(foundMinor);
}

TEST_CASE("Major scale pitch class set is correct", "[music_theory]") {
    // C major: C D E F G A B = semitones 0 2 4 5 7 9 11
    const auto& pcs = Scales::Major.intervals;
    CHECK(pcs.contains(0));
    CHECK(pcs.contains(2));
    CHECK(pcs.contains(4));
    CHECK(pcs.contains(5));
    CHECK(pcs.contains(7));
    CHECK(pcs.contains(9));
    CHECK(pcs.contains(11));
    CHECK_FALSE(pcs.contains(1));
    CHECK_FALSE(pcs.contains(3));
    CHECK_FALSE(pcs.contains(6));
    CHECK_FALSE(pcs.contains(8));
    CHECK_FALSE(pcs.contains(10));
}

TEST_CASE("Natural minor scale pitch class set is correct", "[music_theory]") {
    // Natural minor: 0 2 3 5 7 8 10
    const auto& pcs = Scales::NaturalMinor.intervals;
    CHECK(pcs.contains(0));
    CHECK(pcs.contains(2));
    CHECK(pcs.contains(3));
    CHECK(pcs.contains(5));
    CHECK(pcs.contains(7));
    CHECK(pcs.contains(8));
    CHECK(pcs.contains(10));
    CHECK_FALSE(pcs.contains(4));
    CHECK_FALSE(pcs.contains(11));
}

#include <catch2/catch_approx.hpp>

TEST_CASE("midiFromFreq / freqFromMidi roundtrip", "[music_theory]") {
    CHECK(mv::midiFromFreq(440.0) == 69);              // A4
    CHECK(mv::midiFromFreq(261.6256) == 60);           // C4
    CHECK(mv::freqFromMidi(69.0) == Catch::Approx(440.0));
    CHECK(mv::freqFromMidi(60.0) == Catch::Approx(261.6256).margin(0.01));
}

TEST_CASE("midiFromFreq rounds to nearest semitone", "[music_theory]") {
    CHECK(mv::midiFromFreq(443.0) == 69);
    CHECK(mv::midiFromFreq(450.0) == 69);
    CHECK(mv::midiFromFreq(460.0) == 70);
}

TEST_CASE("scaleDegreeOf for C major", "[music_theory]") {
    mv::Key cMajor { mv::NoteName::C, &mv::Scales::Major };
    CHECK(mv::scaleDegreeOf(60, cMajor) == 0);   // C4 -> root (degree 0)
    CHECK(mv::scaleDegreeOf(62, cMajor) == 1);   // D4 -> 2nd
    CHECK(mv::scaleDegreeOf(64, cMajor) == 2);   // E4 -> 3rd
    CHECK(mv::scaleDegreeOf(65, cMajor) == 3);   // F4 -> 4th
    CHECK(mv::scaleDegreeOf(67, cMajor) == 4);   // G4 -> 5th
    CHECK(mv::scaleDegreeOf(69, cMajor) == 5);   // A4 -> 6th
    CHECK(mv::scaleDegreeOf(71, cMajor) == 6);   // B4 -> 7th
    CHECK(mv::scaleDegreeOf(61, cMajor) == -1);  // C#4 -> not in key
    CHECK(mv::scaleDegreeOf(72, cMajor) == 0);   // C5 -> root (different octave, same degree)
}

TEST_CASE("scaleDegreeOf for A natural minor", "[music_theory]") {
    mv::Key aMinor { mv::NoteName::A, &mv::Scales::NaturalMinor };
    CHECK(mv::scaleDegreeOf(69, aMinor) == 0);   // A4 -> root
    CHECK(mv::scaleDegreeOf(71, aMinor) == 1);   // B4 -> 2nd
    CHECK(mv::scaleDegreeOf(72, aMinor) == 2);   // C5 -> 3rd
    CHECK(mv::scaleDegreeOf(70, aMinor) == -1);  // A#4 -> not in key
}

TEST_CASE("quantizeToKey snaps to nearest in-key note", "[music_theory]") {
    mv::Key cMajor { mv::NoteName::C, &mv::Scales::Major };
    CHECK(mv::quantizeToKey(60, cMajor) == 60);  // C4 -> C4 (in key)
    CHECK(mv::quantizeToKey(61, cMajor) == 60);  // C#4 -> C4 (closer than D4)

    // F#4 = 66: equidistant from F4(65) and G4(67). Tie -> prefer lower (deterministic).
    CHECK(mv::quantizeToKey(66, cMajor) == 65);

    // Bb4 = 70: 1 semi from A4(69), 1 semi from B4(71). Tie -> prefer lower.
    CHECK(mv::quantizeToKey(70, cMajor) == 69);
}

TEST_CASE("quantizeToKey across octaves", "[music_theory]") {
    mv::Key cMajor { mv::NoteName::C, &mv::Scales::Major };
    CHECK(mv::quantizeToKey(72, cMajor) == 72);  // C5
    CHECK(mv::quantizeToKey(85, cMajor) == 84);  // C#6 -> C6
}

TEST_CASE("targetForInterval in C major", "[music_theory]") {
    mv::Key cMajor { mv::NoteName::C, &mv::Scales::Major };

    // +2 scale-degree = "diatonic 3rd above"
    CHECK(mv::targetForInterval(60, +2, cMajor) == 64);  // C4 -> E4 (major 3rd)
    CHECK(mv::targetForInterval(62, +2, cMajor) == 65);  // D4 -> F4 (minor 3rd)
    CHECK(mv::targetForInterval(64, +2, cMajor) == 67);  // E4 -> G4 (minor 3rd)
    CHECK(mv::targetForInterval(65, +2, cMajor) == 69);  // F4 -> A4 (major 3rd)
    CHECK(mv::targetForInterval(67, +2, cMajor) == 71);  // G4 -> B4 (major 3rd)

    // +4 = 5th, +7 = octave (7 diatonic degrees in 7-note scale)
    CHECK(mv::targetForInterval(60, +4, cMajor) == 67);  // C4 -> G4 (perfect 5th)
    CHECK(mv::targetForInterval(60, +7, cMajor) == 72);  // C4 -> C5

    // Negative interval = down
    CHECK(mv::targetForInterval(60, -2, cMajor) == 57);  // C4 -> A3 (down a 3rd)
    CHECK(mv::targetForInterval(60, -7, cMajor) == 48);  // C4 -> C3 (octave down)
}

TEST_CASE("targetForInterval in A natural minor", "[music_theory]") {
    mv::Key aMinor { mv::NoteName::A, &mv::Scales::NaturalMinor };

    // C in A-minor at scale-degree +2 -> E (in-key minor 3rd from C in A-natural-minor)
    // A-minor scale notes: A B C D E F G
    // From C, +2 degrees = E.
    CHECK(mv::targetForInterval(60, +2, aMinor) == 64);  // C4 -> E4
}

TEST_CASE("targetForInterval snaps off-key input first", "[music_theory]") {
    mv::Key cMajor { mv::NoteName::C, &mv::Scales::Major };
    // C#4 (61) is off-key -> snap to C4 (60), then +2 -> E4 (64).
    CHECK(mv::targetForInterval(61, +2, cMajor) == 64);
}
