#include "RadialKnob.h"

namespace mv::ui {
using namespace Theme;

RadialKnob::RadialKnob(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
                        juce::String label, juce::Colour accent,
                        std::function<juce::String(double)> formatter)
    : labelText(std::move(label)), valueFormatter(std::move(formatter)) {
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, borderSoft);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    addAndMakeVisible(slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID, slider);
    slider.onValueChange = [this] { repaint(); };
}

void RadialKnob::resized() {
    auto b = getLocalBounds();
    b.removeFromTop(14);
    b.removeFromBottom(14);
    const int d = juce::jmin(b.getWidth(), b.getHeight());
    slider.setBounds(b.withSizeKeepingCentre(d, d));
}

void RadialKnob::paint(juce::Graphics& g) {
    auto b = getLocalBounds();

    g.setColour(textFaint);
    g.setFont(uiFont(10.0f, true));
    g.drawText(labelText.toUpperCase(), b.removeFromTop(14), juce::Justification::centred);

    g.setColour(textDim);
    g.setFont(monoFont(10.0f));
    auto valueText = valueFormatter ? valueFormatter(slider.getValue())
                                     : juce::String(slider.getValue(), 1);
    g.drawText(valueText, b.removeFromBottom(14), juce::Justification::centred);
}

} // namespace mv::ui
