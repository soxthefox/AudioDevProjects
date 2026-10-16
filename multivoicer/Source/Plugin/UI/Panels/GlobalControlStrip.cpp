#include "GlobalControlStrip.h"
#include "../../Parameters.h"

namespace mv::ui {
using namespace Theme;

GlobalControlStrip::GlobalControlStrip(juce::AudioProcessorValueTreeState& apvts)
    : voiceStepper(apvts, mv::Params::kVoiceCount, "Voices"),
      formantToggle(apvts, mv::Params::kFormantPreserve, brand),
      dryWetKnob(apvts, mv::Params::kDryWetMix, "Dry / Wet", brand,
          [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; }) {
    populateChoiceBox(keyBox, apvts, mv::Params::kKeyRoot);
    populateChoiceBox(scaleBox, apvts, mv::Params::kKeyScale);
    populateChoiceBox(presetBox, apvts, mv::Params::kPreset);
    keyAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, mv::Params::kKeyRoot, keyBox);
    scaleAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, mv::Params::kKeyScale, scaleBox);
    presetAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, mv::Params::kPreset, presetBox);

    for (auto* c : { &keyBox, &scaleBox, &presetBox }) {
        c->setColour(juce::ComboBox::backgroundColourId, panelAlt);
        c->setColour(juce::ComboBox::outlineColourId, borderSoft);
        c->setColour(juce::ComboBox::textColourId, text);
        c->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(c);
    }
    presetBox.setColour(juce::ComboBox::outlineColourId, brand);
    presetBox.setColour(juce::ComboBox::backgroundColourId, brand.withAlpha(0.12f));

    addAndMakeVisible(voiceStepper);
    addAndMakeVisible(formantToggle);
    addAndMakeVisible(dryWetKnob);

    if (auto* p = apvts.getParameter(mv::Params::kMode))
        modeAtt = std::make_unique<juce::ParameterAttachment>(*p, [this](float v) {
            midiMode = juce::roundToInt(v) == 1;
            keyBox.setEnabled(!midiMode);
            scaleBox.setEnabled(!midiMode);
            keyBox.setAlpha(midiMode ? 0.4f : 1.0f);
            scaleBox.setAlpha(midiMode ? 0.4f : 1.0f);
            repaint();
        });
    if (modeAtt) modeAtt->sendInitialUpdate();
}

void GlobalControlStrip::populateChoiceBox(juce::ComboBox& box, juce::AudioProcessorValueTreeState& apvts,
                                            const juce::String& paramID) {
    auto* param = apvts.getParameter(paramID);
    jassert(param != nullptr);
    auto choices = param->getAllValueStrings();
    for (int i = 0; i < choices.size(); ++i)
        box.addItem(choices[i], i + 1);
}

void GlobalControlStrip::resized() {
    auto b = getLocalBounds().reduced(20, 14);
    constexpr int capH = 14, ctrlH = 34, gap = 20;

    auto takeCol = [&](int w) { auto col = b.removeFromLeft(w); b.removeFromLeft(gap); return col; };

    auto keyCol = takeCol(70);
    keyLabelR = keyCol.removeFromTop(capH);
    keyBox.setBounds(keyCol.withHeight(ctrlH));

    auto scaleCol = takeCol(92);
    scaleLabelR = scaleCol.removeFromTop(capH);
    scaleBox.setBounds(scaleCol.withHeight(ctrlH));

    voiceStepper.setBounds(takeCol(72));

    auto formantCol = takeCol(56);
    formantLabelR = formantCol.removeFromTop(capH);
    formantCol.removeFromTop(6);
    formantToggle.setBounds(formantCol.removeFromTop(18).withSizeKeepingCentre(34, 18));

    dryWetKnob.setBounds(takeCol(56).withHeight(68));

    presetLabelR = b.removeFromTop(capH);
    presetBox.setBounds(b.withHeight(ctrlH));
}

void GlobalControlStrip::paint(juce::Graphics& g) {
    auto b = getLocalBounds().toFloat();
    g.setColour(panel);
    g.fillRoundedRectangle(b, (float) Spacing::radius);
    g.setColour(borderSoft);
    g.drawRoundedRectangle(b.reduced(0.5f), (float) Spacing::radius, 1.0f);

    g.setFont(uiFont(10.0f, true));
    g.setColour(textFaint);
    g.drawText("KEY", keyLabelR, juce::Justification::centredLeft);
    g.drawText("SCALE", scaleLabelR, juce::Justification::centredLeft);
    g.drawText("FORMANT", formantLabelR, juce::Justification::centredLeft);
    g.setColour(midiMode ? brand : textFaint);
    g.drawText("PRESET", presetLabelR, juce::Justification::centredLeft);
}

} // namespace mv::ui
