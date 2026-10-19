#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Theme.h"

namespace mv::ui {

// Pill on/off switch bound to a bool APVTS param. Drawing lives in
// MultivoicerLookAndFeel::drawToggleButton; this just wires the attachment.
class ToggleSwitch : public juce::Component {
public:
    ToggleSwitch(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
                 juce::Colour accent = Theme::brand);

    void resized() override { button.setBounds(getLocalBounds()); }

    juce::ToggleButton button;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToggleSwitch)
};

} // namespace mv::ui
