#include "PluginEditor.h"
#include "UI/Theme.h"

using namespace mv::ui;

MultivoicerEditor::MultivoicerEditor(MultivoicerProcessor& p)
    : juce::AudioProcessorEditor(p),
      header(p.apvts, p.uiMeter.inputLevel, p.uiMeter.voiceLevel),
      globalStrip(p.apvts),
      triggerRow(p.apvts),
      midiRow(p.apvts, p.uiMeter.voiceMidiNote[0]),
      footer(p) {
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(header);
    addAndMakeVisible(globalStrip);
    addAndMakeVisible(triggerRow);
    addAndMakeVisible(midiRow);

    for (int i = 0; i < 4; ++i) {
        voiceCards[(size_t) i] = std::make_unique<VoiceCard>(
            p.apvts, i + 1, Theme::voiceColour(i), p.uiMeter.voiceMidiNote[(size_t) i]);
        addAndMakeVisible(*voiceCards[(size_t) i]);
    }
    addAndMakeVisible(footer);

    if (auto* modeParam = p.apvts.getParameter(mv::Params::kMode))
        modeAtt = std::make_unique<juce::ParameterAttachment>(*modeParam, [this](float v) {
            const bool midi = juce::roundToInt(v) == 1;
            triggerRow.setVisible(!midi);
            midiRow.setVisible(midi);
        });
    if (modeAtt) modeAtt->sendInitialUpdate();

    setResizable(true, true);
    setResizeLimits(760, 700, 1400, 1200);
    setSize(920, 820);
}

MultivoicerEditor::~MultivoicerEditor() { setLookAndFeel(nullptr); }

void MultivoicerEditor::resized() {
    auto b = getLocalBounds();

    header.setBounds(b.removeFromTop(120));

    auto content = b.reduced(20, 0);
    content.removeFromTop(16);
    globalStrip.setBounds(content.removeFromTop(80));
    content.removeFromTop(16);

    auto modeRowArea = content.removeFromTop(50);
    triggerRow.setBounds(modeRowArea);
    midiRow.setBounds(modeRowArea);
    content.removeFromTop(16);

    auto footerArea = content.removeFromBottom(44);
    content.removeFromBottom(16);
    footer.setBounds(footerArea);

    auto cardsArea = content;
    const int gap = 16;
    const int cardW = (cardsArea.getWidth() - gap * 3) / 4;
    for (int i = 0; i < 4; ++i) {
        voiceCards[(size_t) i]->setBounds(cardsArea.removeFromLeft(cardW));
        cardsArea.removeFromLeft(gap);
    }
}
