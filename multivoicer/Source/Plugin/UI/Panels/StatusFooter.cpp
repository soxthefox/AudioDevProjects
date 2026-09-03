#include "StatusFooter.h"
#include "../Theme.h"
#include "../MusicLabels.h"
#include "../../../Music/MusicTheory.h"

namespace mv::ui {
using namespace Theme;

StatusFooter::StatusFooter(MultivoicerProcessor& processor) : proc(processor) { startTimerHz(12); }
StatusFooter::~StatusFooter() { stopTimer(); }

void StatusFooter::timerCallback() { repaint(); }

void StatusFooter::paint(juce::Graphics& g) {
    auto b = getLocalBounds().toFloat();
    g.setColour(panel);
    g.fillRoundedRectangle(b, (float) Spacing::radius);
    g.setColour(borderSoft);
    g.drawRoundedRectangle(b.reduced(0.5f), (float) Spacing::radius, 1.0f);

    auto row = getLocalBounds().reduced(20, 8);

    const bool voiced = proc.uiMeter.voiced.load(std::memory_order_relaxed);
    const float freq = proc.uiMeter.inputFreqHz.load(std::memory_order_relaxed);

    g.setColour(voiced ? voiceColour(0) : textFaint);
    g.fillEllipse(row.removeFromLeft(10).withSizeKeepingCentre(7, 7).toFloat());
    row.removeFromLeft(8);

    g.setFont(monoFont(11.0f));
    g.setColour(textDim);
    juce::String status = voiced
        ? "INPUT DETECTED  *  " + Labels::midiNoteName(mv::midiFromFreq((double) freq)) + "  *  " + juce::String(freq, 1) + " Hz"
        : juce::String("NO INPUT DETECTED");
    g.drawText(status, row, juce::Justification::centredLeft);

    const double latencyMs = proc.getSampleRate() > 0.0
        ? 1000.0 * (double) proc.getLatencySamples() / proc.getSampleRate()
        : 0.0;
    g.setColour(textFaint);
    g.drawText("Latency ~" + juce::String(latencyMs, 1) + " ms", getLocalBounds().reduced(20, 8), juce::Justification::centredRight);
}

} // namespace mv::ui
