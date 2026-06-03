#include "PresetBank.h"
#include "../Music/Scales.h"

namespace mv {

static VoiceParams basicVoice(int interval, float pan = 0.0f) {
    VoiceParams v;
    v.enabled = true;
    v.interval = interval;
    v.gainDb = 0.0f;
    v.pan = pan;
    v.attack = 20.0f; v.decay = 80.0f; v.sustain = 0.8f; v.release = 300.0f;
    v.lowShelfDb = 0.0f; v.midPeakDb = 0.0f; v.highShelfDb = 0.0f;
    v.midFreqHz = 1000.0f; v.midQ = 1.0f;
    return v;
}

static Preset preset(juce::String name) {
    Preset p;
    p.name = std::move(name);
    p.mode = Mode::Preset;
    p.key = { NoteName::C, &Scales::Major };
    p.formantPreserve = true;
    p.dryWetMix = 0.5f;
    p.trigger = PresetTrigger::InputGate;
    p.inputGateThresholdDb = -40.0f;
    p.inputGateHoldMs = 50.0f;
    p.pitchConfidenceMin = 0.5f;
    p.monoMidiMode = false;
    return p;
}

std::vector<Preset> PresetBank::defaults() {
    std::vector<Preset> bank;

    // 1. Octave Up
    {
        auto p = preset("Octave Up");
        p.numActiveVoices = 1;
        p.voices[0] = basicVoice(+7);
        bank.push_back(p);
    }
    // 2. Octave Down
    {
        auto p = preset("Octave Down");
        p.numActiveVoices = 1;
        p.voices[0] = basicVoice(-7);
        bank.push_back(p);
    }
    // 3. Third Up
    {
        auto p = preset("Third Up");
        p.numActiveVoices = 1;
        p.voices[0] = basicVoice(+2);
        bank.push_back(p);
    }
    // 4. 3rd + 5th Above
    {
        auto p = preset("3rd + 5th Above");
        p.numActiveVoices = 2;
        p.voices[0] = basicVoice(+2, -0.3f);
        p.voices[1] = basicVoice(+4, +0.3f);
        bank.push_back(p);
    }
    // 5. Full Triad
    {
        auto p = preset("Full Triad");
        p.numActiveVoices = 3;
        p.voices[0] = basicVoice(+2, -0.4f);
        p.voices[1] = basicVoice(+4, +0.4f);
        p.voices[2] = basicVoice(+7, 0.0f);
        bank.push_back(p);
    }
    // 6. Talkbox Mono
    {
        auto p = preset("Talkbox Mono");
        p.mode = Mode::Midi;
        p.numActiveVoices = 1;
        p.monoMidiMode = true;
        p.formantPreserve = true;
        p.dryWetMix = 1.0f;
        p.voices[0] = basicVoice(0);
        p.voices[0].attack = 10.0f; p.voices[0].release = 100.0f;
        bank.push_back(p);
    }
    return bank;
}

} // namespace mv
