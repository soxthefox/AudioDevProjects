#include "ToggleSwitch.h"

namespace mv::ui {

ToggleSwitch::ToggleSwitch(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
                            juce::Colour accent) {
    button.setColour(juce::ToggleButton::tickColourId, accent);
    addAndMakeVisible(button);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, paramID, button);
}

} // namespace mv::ui
