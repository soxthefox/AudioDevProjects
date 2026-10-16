#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Controls/ToggleSwitch.h"

namespace mv::ui {

// MIDI-mode-only: mono/poly toggle plus a live view of what the allocator
// is actually doing (reads MultivoicerProcessor::UiMeter::heldMidiNote).
class MidiStatusRow : public juce::Component, private juce::Timer {
public:
    MidiStatusRow(juce::AudioProcessorValueTreeState& apvts, const std::atomic<int>& heldNote);
    ~MidiStatusRow() override;

    void resized() override;
    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;

    ToggleSwitch monoToggle;
    const std::atomic<int>& heldNoteRef;
    float pulsePhase = 0.0f;
    juce::Rectangle<int> monoLabelR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiStatusRow)
};

} // namespace mv::ui
