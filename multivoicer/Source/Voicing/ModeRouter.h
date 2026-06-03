#pragma once

#include <array>
#include "MidiVoiceAllocator.h"
#include "../Music/MusicTheory.h"

namespace mv {

enum class Mode          { Preset, Midi };
enum class PresetTrigger { InputGate, PitchGate, AlwaysOn };

struct VoiceDecision {
    bool   active = false;
    double targetFreqHz = 0.0;
    bool   gateOn = false;
};

class ModeRouter {
public:
    struct Inputs {
        Mode          mode = Mode::Preset;
        PresetTrigger trigger = PresetTrigger::InputGate;
        Key           key { NoteName::C, nullptr };
        int           numActiveVoices = 3;
        std::array<int,4> voiceIntervals { 0,0,0,0 };
        double        inputFreqHz = 0.0;
        bool          voiced = false;
        float         dryRMS = 0.0f;
        float         inputGateThresholdDb = -40.0f;
        float         pitchConfidenceMin = 0.5f;
    };

    std::array<VoiceDecision, 4> decide(
        const Inputs& in,
        const std::array<MidiVoiceAllocator::Slot, 4>& midiSlots);
};

} // namespace mv
