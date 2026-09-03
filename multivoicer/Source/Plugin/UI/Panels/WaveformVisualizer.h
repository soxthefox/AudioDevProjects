#pragma once

#include <array>
#include <atomic>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Theme.h"

namespace mv::ui {

// Chaotic layered waveform behind the header. Amplitude per line tracks the
// real per-voice output level reported by the processor (see UiMeter) — an
// idle voice draws as a near-flat line, an active one animates.
class WaveformVisualizer : public juce::Component, private juce::Timer {
public:
    WaveformVisualizer(const std::atomic<float>& inputLevel,
                        const std::array<std::atomic<float>, 4>& voiceLevels);
    ~WaveformVisualizer() override;

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;

    const std::atomic<float>& inputLevelRef;
    const std::array<std::atomic<float>, 4>& voiceLevelsRef;
    float phase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformVisualizer)
};

} // namespace mv::ui
