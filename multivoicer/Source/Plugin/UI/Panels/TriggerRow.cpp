#include "TriggerRow.h"
#include "../../Parameters.h"

namespace mv::ui {
using namespace Theme;

TriggerRow::TriggerRow(juce::AudioProcessorValueTreeState& apvts)
    : triggerType(apvts, mv::Params::kPresetTrigger, { "Input Gate", "Pitch Gate", "Always On" }, brand),
      threshold(apvts, mv::Params::kInputGateThresholdDb, "Threshold", brand,
          [](double v) { return juce::String(v, 0) + " dB"; }),
      hold(apvts, mv::Params::kInputGateHoldMs, "Hold", brand,
          [](double v) { return juce::String(v, 0) + " ms"; }),
      confidence(apvts, mv::Params::kPitchConfidenceMin, "Min Confidence", brand,
          [](double v) { return juce::String(v, 2); }) {
    addAndMakeVisible(triggerType);
    addAndMakeVisible(threshold);
    addAndMakeVisible(hold);
    addAndMakeVisible(confidence);
}

void TriggerRow::resized() {
    auto b = getLocalBounds().reduced(20, 8);
    triggerType.setBounds(b.removeFromLeft(220));
    b.removeFromLeft(24);
    threshold.setBounds(b.removeFromLeft(120));
    b.removeFromLeft(20);
    hold.setBounds(b.removeFromLeft(90));
    b.removeFromLeft(20);
    confidence.setBounds(b.removeFromLeft(120));
}

void TriggerRow::paint(juce::Graphics& g) {
    auto b = getLocalBounds().toFloat();
    g.setColour(panel);
    g.fillRoundedRectangle(b, (float) Spacing::radius);
    g.setColour(borderSoft);
    g.drawRoundedRectangle(b.reduced(0.5f), (float) Spacing::radius, 1.0f);
}

} // namespace mv::ui
