#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Theme.h"

namespace mv::ui {

// -/[value]/+ control for small-range int params (voice count and the like).
class Stepper : public juce::Component {
public:
    Stepper(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, juce::String label);

    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::String labelText;
    juce::Rectangle<int> minusBounds, plusBounds;
    float value = 1.0f, minV = 1.0f, maxV = 4.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Stepper)
};

} // namespace mv::ui
