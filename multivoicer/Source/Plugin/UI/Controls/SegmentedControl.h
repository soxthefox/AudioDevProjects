#pragma once

#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Theme.h"

namespace mv::ui {

// Pill-shaped N-way switch for choice params (mode, trigger type, ...).
// Uses the generic ParameterAttachment since it isn't a Slider/ComboBox/Button.
class SegmentedControl : public juce::Component {
public:
    SegmentedControl(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
                      juce::StringArray segmentLabels, juce::Colour accent = Theme::brand);

    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;

    int getCurrentIndex() const { return currentIndex; }
    std::function<void(int)> onChange;

private:
    juce::StringArray labels;
    juce::Colour accentColour;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int currentIndex = 0;
    std::vector<juce::Rectangle<int>> segmentBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SegmentedControl)
};

} // namespace mv::ui
