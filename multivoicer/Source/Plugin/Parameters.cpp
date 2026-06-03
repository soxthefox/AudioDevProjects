#include "Parameters.h"

namespace mv::Params {

juce::String voiceParam(const char* field, int idx) {
    return juce::String("voice") + field + "_" + juce::String(idx);
}

using APVTS = juce::AudioProcessorValueTreeState;
using AParam = juce::AudioProcessorParameter;
using FloatP = juce::AudioParameterFloat;
using IntP   = juce::AudioParameterInt;
using BoolP  = juce::AudioParameterBool;
using ChoiceP = juce::AudioParameterChoice;

static std::unique_ptr<juce::AudioProcessorParameterGroup> makeGlobalGroup() {
    auto g = std::make_unique<juce::AudioProcessorParameterGroup>("global", "Global", "|");
    g->addChild(std::make_unique<ChoiceP>(juce::ParameterID(kMode, 1), "Mode",
        juce::StringArray{"Preset", "MIDI"}, 0));
    g->addChild(std::make_unique<IntP>(juce::ParameterID(kVoiceCount, 1), "Voice Count", 1, 4, 3));
    g->addChild(std::make_unique<ChoiceP>(juce::ParameterID(kKeyRoot, 1), "Key Root",
        juce::StringArray{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}, 0));
    g->addChild(std::make_unique<ChoiceP>(juce::ParameterID(kKeyScale, 1), "Scale",
        juce::StringArray{"Major", "Natural Minor"}, 0));
    g->addChild(std::make_unique<BoolP>(juce::ParameterID(kFormantPreserve, 1), "Formant Preserve", true));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(kDryWetMix, 1), "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    g->addChild(std::make_unique<ChoiceP>(juce::ParameterID(kPreset, 1), "Preset",
        juce::StringArray{"Octave Up","Octave Down","Third Up","3rd + 5th Above","Full Triad","Talkbox Mono"}, 0));
    g->addChild(std::make_unique<BoolP>(juce::ParameterID(kMonoMidiMode, 1), "Mono MIDI", false));
    return g;
}

static std::unique_ptr<juce::AudioProcessorParameterGroup> makePresetTriggerGroup() {
    auto g = std::make_unique<juce::AudioProcessorParameterGroup>("presetTrigger", "Preset Trigger", "|");
    g->addChild(std::make_unique<ChoiceP>(juce::ParameterID(kPresetTrigger, 1), "Trigger",
        juce::StringArray{"InputGate","PitchGate","AlwaysOn"}, 0));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(kInputGateThresholdDb, 1), "Gate Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f), -40.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(kInputGateHoldMs, 1), "Gate Hold",
        juce::NormalisableRange<float>(0.0f, 500.0f), 50.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(kPitchConfidenceMin, 1), "Pitch Confidence Min",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    return g;
}

static std::unique_ptr<juce::AudioProcessorParameterGroup> makeVoiceGroup(int idx, bool enabledDefault, int intervalDefault) {
    auto g = std::make_unique<juce::AudioProcessorParameterGroup>(
        juce::String("voice_") + juce::String(idx),
        juce::String("Voice ") + juce::String(idx), "|");
    g->addChild(std::make_unique<BoolP>(juce::ParameterID(voiceParam("Enabled", idx), 1),       "Enabled",  enabledDefault));
    g->addChild(std::make_unique<IntP>  (juce::ParameterID(voiceParam("Interval", idx), 1),     "Interval", -14, 14, intervalDefault));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("GainDb", idx), 1),       "Gain (dB)", juce::NormalisableRange<float>(-24.0f, 6.0f), 0.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("Pan", idx), 1),          "Pan",       juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("AttackMs", idx), 1),     "Attack (ms)", juce::NormalisableRange<float>(0.0f, 2000.0f), 20.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("DecayMs", idx), 1),      "Decay (ms)",  juce::NormalisableRange<float>(0.0f, 2000.0f), 80.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("Sustain", idx), 1),      "Sustain",     juce::NormalisableRange<float>(0.0f, 1.0f),   0.8f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("ReleaseMs", idx), 1),    "Release (ms)",juce::NormalisableRange<float>(0.0f, 5000.0f), 300.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("EqLowDb", idx), 1),      "EQ Low (dB)", juce::NormalisableRange<float>(-18.0f, 18.0f), 0.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("EqMidDb", idx), 1),      "EQ Mid (dB)", juce::NormalisableRange<float>(-18.0f, 18.0f), 0.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("EqMidHz", idx), 1),      "EQ Mid (Hz)", juce::NormalisableRange<float>(200.0f, 8000.0f, 0.0f, 0.3f), 1000.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("EqMidQ", idx), 1),       "EQ Mid Q",    juce::NormalisableRange<float>(0.1f, 10.0f),  1.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("EqHighDb", idx), 1),     "EQ High (dB)",juce::NormalisableRange<float>(-18.0f, 18.0f), 0.0f));
    return g;
}

APVTS::ParameterLayout buildLayout() {
    APVTS::ParameterLayout layout;
    layout.add(makeGlobalGroup());
    layout.add(makePresetTriggerGroup());
    layout.add(makeVoiceGroup(1, true,  +2));
    layout.add(makeVoiceGroup(2, true,  +4));
    layout.add(makeVoiceGroup(3, true,  +7));
    layout.add(makeVoiceGroup(4, false, +9));
    return layout;
}

} // namespace mv::Params
