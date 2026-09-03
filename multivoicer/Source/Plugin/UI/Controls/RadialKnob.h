#pragma once

#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Theme.h"

namespace mv::ui {

// Self-contained knob: caption above, arc-style rotary dial, live value below.
// Owns its own APVTS attachment so a parent just drops it into a layout.
class RadialKnob : public juce::Component {
public:
    RadialKnob(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
               juce::String label, juce::Colour accent,
               std::function<juce::String(double)> formatter = {});

    void resized() override;
    void paint(juce::Graphics&) override;

private:
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::String labelText;
    std::function<juce::String(double)> valueFormatter;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RadialKnob)
};

} // namespace mv::ui
