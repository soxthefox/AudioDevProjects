#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Controls/ToggleSwitch.h"
#include "../Controls/RadialKnob.h"
#include "../Controls/BipolarSlider.h"
#include "../Controls/EnvelopeGraph.h"
#include "../Controls/EqCurveView.h"

namespace mv::ui {

// One voice: enable, pitch target readout (diatonic interval in Preset mode,
// held MIDI note in MIDI mode), gain/pan, envelope graph, EQ curve.
class VoiceCard : public juce::Component, private juce::Timer {
public:
    VoiceCard(juce::AudioProcessorValueTreeState& apvts, int voiceIndex, juce::Colour accent,
               const std::atomic<int>& voiceMidiNote);
    ~VoiceCard() override;

    void resized() override;
    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;
    juce::String buildReadoutMain() const;
    juce::String buildReadoutCaption() const;

    juce::AudioProcessorValueTreeState& state;
    int index;
    juce::Colour accentColour;
    const std::atomic<int>& midiNoteRef;

    ToggleSwitch enableToggle;
    RadialKnob gainKnob;
    BipolarSlider panSlider;
    EnvelopeGraph envelope;
    EqCurveView eqCurve;

    juce::Rectangle<int> readoutR, envelopeLabelR, envelopeValuesR, eqLabelR, eqValuesR;
    bool cachedMidiMode = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoiceCard)
};

} // namespace mv::ui
