#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Controls/RadialKnob.h"
#include "../Controls/ToggleSwitch.h"
#include "../Controls/Stepper.h"

namespace mv::ui {

// Key / Scale / Voice count / Formant / Dry-Wet / Preset — the row that's
// always visible regardless of mode. Key & Scale dim themselves in MIDI mode
// since the router ignores them there.
class GlobalControlStrip : public juce::Component {
public:
    explicit GlobalControlStrip(juce::AudioProcessorValueTreeState& apvts);

    void resized() override;
    void paint(juce::Graphics&) override;

private:
    void populateChoiceBox(juce::ComboBox&, juce::AudioProcessorValueTreeState&, const juce::String& paramID);

    juce::ComboBox keyBox, scaleBox, presetBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt, presetAtt;
    Stepper voiceStepper;
    ToggleSwitch formantToggle;
    RadialKnob dryWetKnob;
    std::unique_ptr<juce::ParameterAttachment> modeAtt;
    bool midiMode = false;

    juce::Rectangle<int> keyLabelR, scaleLabelR, formantLabelR, presetLabelR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlobalControlStrip)
};

} // namespace mv::ui
