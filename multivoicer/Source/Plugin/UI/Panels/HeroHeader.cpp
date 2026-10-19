#include "HeroHeader.h"
#include "../../Parameters.h"

namespace mv::ui {
using namespace Theme;

HeroHeader::HeroHeader(juce::AudioProcessorValueTreeState& apvts,
                        const std::atomic<float>& inputLevel,
                        const std::array<std::atomic<float>, 4>& voiceLevels)
    : visualizer(inputLevel, voiceLevels),
      modeSwitch(apvts, mv::Params::kMode, { "Preset", "MIDI" }, brand) {
    addAndMakeVisible(visualizer);
    addAndMakeVisible(modeSwitch);
}

void HeroHeader::resized() {
    visualizer.setBounds(getLocalBounds());
    modeSwitch.setBounds(getWidth() - 24 - 140, (getHeight() - 34) / 2, 140, 34);
}

void HeroHeader::paint(juce::Graphics& g) {
    g.setGradientFill(juce::ColourGradient(bg.darker(0.3f), 0, 0, bg, 0, (float) getHeight(), false));
    g.fillAll();
}

void HeroHeader::paintOverChildren(juce::Graphics& g) {
    auto b = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(
        bg.darker(0.4f).withAlpha(0.75f), b.getX(), 0, bg.darker(0.4f).withAlpha(0.05f), b.getWidth() * 0.3f, 0, false));
    g.fillRect(b);
    g.setGradientFill(juce::ColourGradient(
        bg.darker(0.4f).withAlpha(0.05f), b.getWidth() * 0.7f, 0, bg.darker(0.4f).withAlpha(0.75f), b.getRight(), 0, false));
    g.fillRect(b);

    auto chip = juce::Rectangle<float>(24, (b.getHeight() - 46) * 0.5f, 190, 46);
    g.setColour(bg.darker(0.4f).withAlpha(0.55f));
    g.fillRoundedRectangle(chip, 10.0f);
    g.setColour(borderSoft);
    g.drawRoundedRectangle(chip.reduced(0.5f), 10.0f, 1.0f);

    auto barsArea = chip.removeFromLeft(34).reduced(6.0f, 10.0f);
    const float barW = barsArea.getWidth() / 4.0f - 2.0f;
    const float heights[4] = { 0.5f, 0.75f, 1.0f, 0.65f };
    for (int i = 0; i < 4; ++i) {
        const float h = barsArea.getHeight() * heights[i];
        g.setColour(voiceColour(i));
        g.fillRoundedRectangle(barsArea.getX() + (float) i * (barW + 2.0f), barsArea.getBottom() - h, barW, h, 1.0f);
    }

    auto textArea = chip.reduced(6.0f, 4.0f);
    g.setColour(text);
    g.setFont(uiFont(15.0f, true));
    g.drawText("MULTIVOICER", textArea.removeFromTop(textArea.getHeight() * 0.55f), juce::Justification::centredLeft);
    g.setColour(textDim);
    g.setFont(uiFont(10.0f));
    g.drawText("Diatonic Harmonizer", textArea, juce::Justification::centredLeft);
}

} // namespace mv::ui
