#include "SegmentedControl.h"

namespace mv::ui {
using namespace Theme;

SegmentedControl::SegmentedControl(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
                                    juce::StringArray segmentLabels, juce::Colour accent)
    : labels(std::move(segmentLabels)), accentColour(accent) {
    auto* param = apvts.getParameter(paramID);
    jassert(param != nullptr);
    attachment = std::make_unique<juce::ParameterAttachment>(*param,
        [this](float v) { currentIndex = (int) std::round(v); repaint(); });
    attachment->sendInitialUpdate();
}

void SegmentedControl::resized() {
    segmentBounds.clear();
    auto b = getLocalBounds();
    const int n = labels.size();
    if (n == 0) return;
    const int w = b.getWidth() / n;
    for (int i = 0; i < n; ++i)
        segmentBounds.push_back(b.removeFromLeft(i == n - 1 ? b.getWidth() : w));
}

void SegmentedControl::paint(juce::Graphics& g) {
    auto b = getLocalBounds().toFloat();
    g.setColour(panel);
    g.fillRoundedRectangle(b, 10.0f);
    g.setColour(borderSoft);
    g.drawRoundedRectangle(b.reduced(0.5f), 10.0f, 1.0f);

    for (int i = 0; i < (int) segmentBounds.size(); ++i) {
        auto seg = segmentBounds[(size_t) i].toFloat().reduced(3.0f);
        if (i == currentIndex) {
            g.setColour(accentColour);
            g.fillRoundedRectangle(seg, 7.0f);
        }
        g.setColour(i == currentIndex ? brandInk : textDim);
        g.setFont(uiFont(11.0f, true));
        g.drawText(labels[i].toUpperCase(), segmentBounds[(size_t) i], juce::Justification::centred);
    }
}

void SegmentedControl::mouseUp(const juce::MouseEvent& e) {
    for (int i = 0; i < (int) segmentBounds.size(); ++i) {
        if (segmentBounds[(size_t) i].contains(e.getPosition())) {
            if (i != currentIndex) {
                attachment->setValueAsCompleteGesture((float) i);
                if (onChange) onChange(i);
            }
            return;
        }
    }
}

} // namespace mv::ui
