#pragma once

#include <array>
#include <juce_core/juce_core.h>
#include "../Music/MusicTheory.h"
#include "../Voicing/Voice.h"
#include "../Voicing/ModeRouter.h"

namespace mv {

struct Preset {
    juce::String  name;

    Mode          mode = Mode::Preset;
    Key           key { NoteName::C, nullptr };
    int           numActiveVoices = 3;
    bool          monoMidiMode = false;
    bool          formantPreserve = true;
    float         dryWetMix = 0.5f;

    PresetTrigger trigger = PresetTrigger::InputGate;
    float         inputGateThresholdDb = -40.0f;
    float         inputGateHoldMs = 50.0f;
    float         pitchConfidenceMin = 0.5f;

    std::array<VoiceParams, 4> voices {};
};

} // namespace mv
