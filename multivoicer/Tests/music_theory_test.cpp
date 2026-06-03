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
