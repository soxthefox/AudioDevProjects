#include "Stepper.h"

namespace mv::ui {
using namespace Theme;

Stepper::Stepper(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, juce::String label)
    : labelText(std::move(label)) {
    auto* param = apvts.getParameter(paramID);
    jassert(param != nullptr);
    auto range = param->getNormalisableRange();
    minV = range.start;
    maxV = range.end;
    attachment = std::make_unique<juce::ParameterAttachment>(*param, [this](float v) { value = v; repaint(); });
    attachment->sendInitialUpdate();
}

void Stepper::resized() {
    auto b = getLocalBounds();
    b.removeFromTop(14);
    minusBounds = b.removeFromLeft(20);
    plusBounds  = b.removeFromRight(20);
}

void Stepper::paint(juce::Graphics& g) {
    auto full = getLocalBounds();
    g.setColour(textFaint);
    g.setFont(uiFont(10.0f, true));
    g.drawText(labelText.toUpperCase(), full.removeFromTop(14), juce::Justification::centred);

    auto b = full.toFloat();
    g.setColour(panelAlt);
    g.fillRoundedRectangle(b, 8.0f);
    g.setColour(borderSoft);
    g.drawRoundedRectangle(b.reduced(0.5f), 8.0f, 1.0f);

    g.setColour(value > minV ? textDim : textFaint.withAlpha(0.4f));
    g.setFont(uiFont(14.0f, true));
    g.drawText("-", minusBounds, juce::Justification::centred);
    g.setColour(value < maxV ? textDim : textFaint.withAlpha(0.4f));
    g.drawText("+", plusBounds, juce::Justification::centred);

    g.setColour(text);
    g.setFont(monoFont(14.0f, true));
    g.drawText(juce::String((int) value), full, juce::Justification::centred);
}

void Stepper::mouseUp(const juce::MouseEvent& e) {
    if (minusBounds.contains(e.getPosition()) && value > minV)
        attachment->setValueAsCompleteGesture(value - 1.0f);
    else if (plusBounds.contains(e.getPosition()) && value < maxV)
        attachment->setValueAsCompleteGesture(value + 1.0f);
}

} // namespace mv::ui
