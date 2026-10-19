#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "Presets/PresetBank.h"

TEST_CASE("PresetBank ships exactly 6 default presets", "[preset_bank]") {
    auto presets = mv::PresetBank::defaults();
    CHECK(presets.size() == 6);
}

TEST_CASE("Default preset names match spec", "[preset_bank]") {
    auto presets = mv::PresetBank::defaults();
    REQUIRE(presets.size() == 6);
    CHECK(presets[0].name == "Octave Up");
    CHECK(presets[1].name == "Octave Down");
    CHECK(presets[2].name == "Third Up");
    CHECK(presets[3].name == "3rd + 5th Above");
    CHECK(presets[4].name == "Full Triad");
    CHECK(presets[5].name == "Talkbox Mono");
}

TEST_CASE("Talkbox preset is MIDI + mono + formant on", "[preset_bank]") {
    auto presets = mv::PresetBank::defaults();
    REQUIRE(presets.size() == 6);
    const auto& tb = presets[5];
    CHECK(tb.mode == mv::Mode::Midi);
    CHECK(tb.numActiveVoices == 1);
    CHECK(tb.monoMidiMode);
    CHECK(tb.formantPreserve);
}

TEST_CASE("All preset numbers are valid (no NaN, ranges respected)", "[preset_bank]") {
    auto presets = mv::PresetBank::defaults();
    for (const auto& p : presets) {
        INFO("Preset: " << p.name.toStdString());
        CHECK(p.numActiveVoices >= 1);
        CHECK(p.numActiveVoices <= 4);
        CHECK(p.dryWetMix >= 0.0f);
        CHECK(p.dryWetMix <= 1.0f);
        for (const auto& v : p.voices) {
            CHECK_FALSE(std::isnan(v.gainDb));
            CHECK_FALSE(std::isnan(v.attack));
            CHECK(v.sustain >= 0.0f); CHECK(v.sustain <= 1.0f);
        }
    }
}
