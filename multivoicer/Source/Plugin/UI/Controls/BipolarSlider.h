#pragma once

#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Theme.h"

namespace mv::ui {

// Centre-tick linear slider for bipolar params (pan and the like). Same
// caption/value framing as RadialKnob so the two mix in a layout cleanly.
class BipolarSlider : public juce::Component {
public:
    BipolarSlider(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
                   juce::String label, juce::Colour accent,
                   std::function<juce::String(double)> formatter = {});

    void resized() override;
    void paint(juce::Graphics&) override;

private:
    juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::String labelText;
    std::function<juce::String(double)> valueFormatter;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BipolarSlider)
};

} // namespace mv::ui
