#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace mv::Params {

// Global IDs
inline constexpr const char* kMode             = "mode";
inline constexpr const char* kVoiceCount       = "voiceCount";
inline constexpr const char* kKeyRoot          = "keyRoot";
inline constexpr const char* kKeyScale         = "keyScale";
inline constexpr const char* kFormantPreserve  = "formantPreserve";
inline constexpr const char* kDryWetMix        = "dryWetMix";
inline constexpr const char* kPreset           = "preset";
inline constexpr const char* kMonoMidiMode     = "monoMidiMode";

// Preset-mode-specific
inline constexpr const char* kPresetTrigger        = "presetTrigger";
inline constexpr const char* kInputGateThresholdDb = "inputGateThresholdDb";
inline constexpr const char* kInputGateHoldMs      = "inputGateHoldMs";
inline constexpr const char* kPitchConfidenceMin   = "pitchConfidenceMin";

// Per voice — pattern: "voice<Field>_<1..4>"
juce::String voiceParam(const char* field, int idx);  // e.g. voiceParam("Enabled", 1) -> "voiceEnabled_1"

juce::AudioProcessorValueTreeState::ParameterLayout buildLayout();

} // namespace mv::Params
