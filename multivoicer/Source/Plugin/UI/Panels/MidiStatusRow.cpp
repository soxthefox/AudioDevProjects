#include "MidiStatusRow.h"
#include "../../Parameters.h"
#include "../MusicLabels.h"
#include <cmath>

namespace mv::ui {
using namespace Theme;

MidiStatusRow::MidiStatusRow(juce::AudioProcessorValueTreeState& apvts, const std::atomic<int>& heldNote)
    : monoToggle(apvts, mv::Params::kMonoMidiMode, brand), heldNoteRef(heldNote) {
    addAndMakeVisible(monoToggle);
    startTimerHz(15);
}

MidiStatusRow::~MidiStatusRow() { stopTimer(); }

void MidiStatusRow::timerCallback() {
    if (heldNoteRef.load(std::memory_order_relaxed) >= 0) pulsePhase += 0.35f;
    repaint();
}

void MidiStatusRow::resized() {
    auto b = getLocalBounds().reduced(20, 8);
    b.removeFromLeft(60);
    monoLabelR = b.removeFromLeft(90);
    monoToggle.setBounds(monoLabelR.removeFromRight(34).withSizeKeepingCentre(34, 18));
}

void MidiStatusRow::paint(juce::Graphics& g) {
    auto b = getLocalBounds().toFloat();
    g.setColour(panel);
    g.fillRoundedRectangle(b, (float) Spacing::radius);
    g.setColour(borderSoft);
    g.drawRoundedRectangle(b.reduced(0.5f), (float) Spacing::radius, 1.0f);

    auto row = getLocalBounds().reduced(20, 8);
    g.setFont(uiFont(10.0f, true));
    g.setColour(textFaint);
    g.drawText("MIDI", row.removeFromLeft(60), juce::Justification::centredLeft);

    g.setColour(textFaint);
    g.drawText("MONO MODE", monoLabelR, juce::Justification::centredLeft);
    row.removeFromLeft(90 + 24);

    const int heldNote = heldNoteRef.load(std::memory_order_relaxed);
    const bool active = heldNote >= 0;
    const float pulse = active ? 0.6f + 0.4f * std::sin(pulsePhase) : 1.0f;
    auto dotArea = row.removeFromLeft(10).withSizeKeepingCentre(7, 7);
    g.setColour((active ? voiceColour(0) : textFaint).withAlpha(pulse));
    g.fillEllipse(dotArea.toFloat());
    row.removeFromLeft(8);
    g.setColour(active ? voiceColour(0) : textFaint);
    g.setFont(monoFont(11.0f, true));
    g.drawText("MIDI IN", row.removeFromLeft(60), juce::Justification::centredLeft);
    row.removeFromLeft(24);

    g.setColour(textFaint);
    g.setFont(uiFont(10.0f, true));
    g.drawText("HELD NOTE", row.removeFromLeft(80), juce::Justification::centredLeft);
    g.setColour(text);
    g.setFont(monoFont(13.0f, true));
    g.drawText(Labels::midiNoteName(heldNote), row.removeFromLeft(50), juce::Justification::centredLeft);

    g.setColour(textFaint);
    g.setFont(uiFont(10.0f, true));
    g.drawText("CHANNEL", row.removeFromRight(90), juce::Justification::centredRight);
    row.removeFromRight(4);
    g.setColour(textDim);
    g.setFont(monoFont(11.0f));
    g.drawText("All", row.removeFromRight(40), juce::Justification::centredRight);
}

} // namespace mv::ui
