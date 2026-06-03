#include <catch2/catch_test_macros.hpp>
#include "Voicing/ModeRouter.h"
#include "Voicing/MidiVoiceAllocator.h"
#include "Music/Scales.h"

TEST_CASE("Preset mode + AlwaysOn + voiced -> all enabled voices active", "[router]") {
    mv::ModeRouter r;
    mv::ModeRouter::Inputs in {};
    in.mode = mv::Mode::Preset;
    in.trigger = mv::PresetTrigger::AlwaysOn;
    in.key = { mv::NoteName::C, &mv::Scales::Major };
    in.inputFreqHz = 261.63; in.voiced = true; in.dryRMS = 0.5f;
    in.numActiveVoices = 3;
    in.voiceIntervals = { 2, 4, 7, 0 };
    auto d = r.decide(in, {});
    for (int i = 0; i < 3; ++i) { CHECK(d[i].active); CHECK(d[i].gateOn); }
    CHECK_FALSE(d[3].active);
}

TEST_CASE("Preset mode + InputGate + RMS below threshold -> gates off", "[router]") {
    mv::ModeRouter r;
    mv::ModeRouter::Inputs in {};
    in.mode = mv::Mode::Preset;
    in.trigger = mv::PresetTrigger::InputGate;
    in.inputGateThresholdDb = -40.0f;
    in.dryRMS = 0.001f;       // way below -40 dB
    in.voiced = true;
    in.numActiveVoices = 2;
    in.key = { mv::NoteName::C, &mv::Scales::Major };
    in.voiceIntervals = { 2, 4, 0, 0 };
    in.inputFreqHz = 261.63;
    auto d = r.decide(in, {});
    for (int i = 0; i < 2; ++i) { CHECK(d[i].active); CHECK_FALSE(d[i].gateOn); }
}

TEST_CASE("Preset mode + PitchGate + unvoiced -> gates off", "[router]") {
    mv::ModeRouter r;
    mv::ModeRouter::Inputs in {};
    in.mode = mv::Mode::Preset;
    in.trigger = mv::PresetTrigger::PitchGate;
    in.voiced = false;
    in.numActiveVoices = 2;
    in.key = { mv::NoteName::C, &mv::Scales::Major };
    in.voiceIntervals = { 2, 4, 0, 0 };
    in.inputFreqHz = 261.63;
    auto d = r.decide(in, {});
    for (int i = 0; i < 2; ++i) CHECK_FALSE(d[i].gateOn);
}

TEST_CASE("MIDI mode: 2 notes held, 4 voices enabled -> 2 active 2 inactive", "[router]") {
    mv::ModeRouter r;
    mv::MidiVoiceAllocator a;
    a.setPoly(4);
    a.noteOn(60); a.tick(); a.noteOn(64);

    mv::ModeRouter::Inputs in {};
    in.mode = mv::Mode::Midi;
    in.numActiveVoices = 4;
    in.voiced = true; in.inputFreqHz = 261.63;
    in.key = { mv::NoteName::C, &mv::Scales::Major };
    auto d = r.decide(in, a.snapshot());

    int active = 0; for (auto& v : d) if (v.gateOn) ++active;
    CHECK(active == 2);
}

TEST_CASE("MIDI mode + no notes -> all gates off", "[router]") {
    mv::ModeRouter r;
    mv::MidiVoiceAllocator a;
    a.setPoly(4);
    mv::ModeRouter::Inputs in {};
    in.mode = mv::Mode::Midi;
    in.numActiveVoices = 4;
    in.voiced = true; in.inputFreqHz = 440.0;
    in.key = { mv::NoteName::C, &mv::Scales::Major };
    auto d = r.decide(in, a.snapshot());
    for (auto& v : d) CHECK_FALSE(v.gateOn);
}
