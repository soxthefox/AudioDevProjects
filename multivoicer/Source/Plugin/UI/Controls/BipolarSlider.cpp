#include "BipolarSlider.h"

namespace mv::ui {
using namespace Theme;

BipolarSlider::BipolarSlider(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
                              juce::String label, juce::Colour accent,
                              std::function<juce::String(double)> formatter)
    : labelText(std::move(label)), valueFormatter(std::move(formatter)) {
    slider.setColour(juce::Slider::backgroundColourId, borderSoft);
    slider.setColour(juce::Slider::thumbColourId, accent);
    addAndMakeVisible(slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID, slider);
    slider.onValueChange = [this] { repaint(); };
}

void BipolarSlider::resized() {
    auto b = getLocalBounds();
    b.removeFromTop(14);
    b.removeFromBottom(14);
    slider.setBounds(b.reduced(4, 0));
}

void BipolarSlider::paint(juce::Graphics& g) {
    auto b = getLocalBounds();

    g.setColour(textFaint);
    g.setFont(uiFont(10.0f, true));
    g.drawText(labelText.toUpperCase(), b.removeFromTop(14), juce::Justification::centred);

    g.setColour(textDim);
    g.setFont(monoFont(10.0f));
    auto valueText = valueFormatter ? valueFormatter(slider.getValue())
                                     : juce::String(slider.getValue(), 2);
    g.drawText(valueText, b.removeFromBottom(14), juce::Justification::centred);
}

} // namespace mv::ui
