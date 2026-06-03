#include "ModeRouter.h"
#include <cmath>

namespace mv {

static bool inputGateOpen(float dryRMS, float thresholdDb) {
    float thresholdLinear = std::pow(10.0f, thresholdDb / 20.0f);
    return dryRMS >= thresholdLinear;
}

std::array<VoiceDecision, 4> ModeRouter::decide(
    const Inputs& in,
    const std::array<MidiVoiceAllocator::Slot, 4>& midiSlots)
{
    std::array<VoiceDecision, 4> out {};
    if (in.mode == Mode::Preset) {
        bool gate = false;
        switch (in.trigger) {
            case PresetTrigger::AlwaysOn:   gate = true; break;
            case PresetTrigger::PitchGate:  gate = in.voiced; break;
            case PresetTrigger::InputGate:  gate = inputGateOpen(in.dryRMS, in.inputGateThresholdDb); break;
        }
        int inputMidi = (in.voiced && in.inputFreqHz > 1.0) ? midiFromFreq(in.inputFreqHz) : 60;
        for (int i = 0; i < 4; ++i) {
            if (i < in.numActiveVoices) {
                out[i].active = true;
                int target = (in.key.scale != nullptr)
                    ? targetForInterval(inputMidi, in.voiceIntervals[i], in.key)
                    : inputMidi + in.voiceIntervals[i];
                out[i].targetFreqHz = freqFromMidi(target);
                out[i].gateOn = gate;
            }
        }
    } else {
        for (int i = 0; i < 4; ++i) {
            if (i < in.numActiveVoices && midiSlots[i].active) {
                out[i].active = true;
                out[i].targetFreqHz = freqFromMidi(midiSlots[i].midiNote);
                out[i].gateOn = true;
            } else if (i < in.numActiveVoices) {
                out[i].active = true;
                out[i].targetFreqHz = 0.0;
                out[i].gateOn = false;
            }
        }
    }
    return out;
}

} // namespace mv
