#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Controls/SegmentedControl.h"
#include "../Controls/BipolarSlider.h"

namespace mv::ui {

// Preset-mode-only: how a voice's gate opens (input level, pitch, or always).
class TriggerRow : public juce::Component {
public:
    explicit TriggerRow(juce::AudioProcessorValueTreeState& apvts);

    void resized() override;
    void paint(juce::Graphics&) override;

private:
    SegmentedControl triggerType;
    BipolarSlider threshold, hold, confidence;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TriggerRow)
};

} // namespace mv::ui
