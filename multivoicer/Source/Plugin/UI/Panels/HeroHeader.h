#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "WaveformVisualizer.h"
#include "../Controls/SegmentedControl.h"

namespace mv::ui {

// Logo + Preset/MIDI mode switch, laid over the audio-reactive waveform.
class HeroHeader : public juce::Component {
public:
    HeroHeader(juce::AudioProcessorValueTreeState& apvts,
               const std::atomic<float>& inputLevel,
               const std::array<std::atomic<float>, 4>& voiceLevels);

    void resized() override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;

private:
    WaveformVisualizer visualizer;
    SegmentedControl modeSwitch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeroHeader)
};

} // namespace mv::ui
