#include "VoiceCard.h"
#include "../../Parameters.h"
#include "../MusicLabels.h"
#include "../../../Music/Scales.h"

namespace mv::ui {
using namespace Theme;

VoiceCard::VoiceCard(juce::AudioProcessorValueTreeState& apvts, int voiceIndex, juce::Colour accent,
                      const std::atomic<int>& voiceMidiNote)
    : state(apvts), index(voiceIndex), accentColour(accent), midiNoteRef(voiceMidiNote),
      enableToggle(apvts, mv::Params::voiceParam("Enabled", voiceIndex), accent),
      gainKnob(apvts, mv::Params::voiceParam("GainDb", voiceIndex), "Gain", accent,
          [](double v) { return (v >= 0 ? "+" : "") + juce::String(v, 1) + " dB"; }),
      panSlider(apvts, mv::Params::voiceParam("Pan", voiceIndex), "Pan", accent,
          [](double v) {
              if (std::abs(v) < 0.01) return juce::String("C");
              return (v < 0 ? "L" : "R") + juce::String(juce::roundToInt(std::abs(v) * 100.0));
          }),
      envelope(apvts, voiceIndex, accent),
      eqCurve(apvts, voiceIndex, accent) {
    addAndMakeVisible(enableToggle);
    addAndMakeVisible(gainKnob);
    addAndMakeVisible(panSlider);
    addAndMakeVisible(envelope);
    addAndMakeVisible(eqCurve);
    startTimerHz(10);
}

VoiceCard::~VoiceCard() { stopTimer(); }

void VoiceCard::timerCallback() {
    cachedMidiMode = (int) *state.getRawParameterValue(mv::Params::kMode) == 1;
    // Full repaint: the ADSR/EQ caption rows below the graphs are drawn here
    // too and need to track live drag values, not just the readout box.
    repaint();
}

juce::String VoiceCard::buildReadoutMain() const {
    if (cachedMidiMode) {
        const int note = midiNoteRef.load(std::memory_order_relaxed);
        return note >= 0 ? Labels::midiNoteName(note) : "--";
    }
    const int interval = (int) *state.getRawParameterValue(mv::Params::voiceParam("Interval", index));
    return (interval >= 0 ? "+" : "") + juce::String(interval);
}

juce::String VoiceCard::buildReadoutCaption() const {
    if (cachedMidiMode) {
        const int note = midiNoteRef.load(std::memory_order_relaxed);
        if (note < 0) return "NO NOTE HELD";
        const bool formant = *state.getRawParameterValue(mv::Params::kFormantPreserve) > 0.5f;
        return juce::String(mv::freqFromMidi(note), 1) + " Hz" + (formant ? " * FORMANT" : "");
    }
    const int rootIdx  = (int) *state.getRawParameterValue(mv::Params::kKeyRoot);
    const int scaleIdx = (int) *state.getRawParameterValue(mv::Params::kKeyScale);
    mv::Key key { (mv::NoteName) rootIdx, mv::Scales::All[juce::jlimit(0, (int) mv::Scales::All.size() - 1, scaleIdx)] };
    const int interval = (int) *state.getRawParameterValue(mv::Params::voiceParam("Interval", index));
    auto [note, quality] = Labels::intervalTarget(key, interval);
    return "-> " + note + "  " + quality;
}

void VoiceCard::resized() {
    auto b = getLocalBounds().reduced(16);

    auto header = b.removeFromTop(20);
    enableToggle.setBounds(header.removeFromRight(34).withSizeKeepingCentre(34, 18));
    b.removeFromTop(10);

    readoutR = b.removeFromTop(54);
    b.removeFromTop(10);

    auto knobsRow = b.removeFromTop(70);
    gainKnob.setBounds(knobsRow.removeFromLeft(knobsRow.getWidth() / 2));
    panSlider.setBounds(knobsRow);
    b.removeFromTop(10);

    // Envelope and EQ share whatever height is left, so a tall window fills
    // out instead of leaving dead space below a fixed-size EQ strip.
    const int labelsAndGaps = 14 + 12 + 8 + 14 + 12;
    const int remaining = juce::jmax(84, b.getHeight() - labelsAndGaps);
    const int envelopeH = juce::jmax(60, juce::roundToInt((float) remaining * 0.62f));
    const int eqH = juce::jmax(24, remaining - envelopeH);

    envelopeLabelR = b.removeFromTop(14);
    envelope.setBounds(b.removeFromTop(envelopeH));
    envelopeValuesR = b.removeFromTop(12);
    b.removeFromTop(8);

    eqLabelR = b.removeFromTop(14);
    eqCurve.setBounds(b.removeFromTop(eqH));
    eqValuesR = b.removeFromTop(12);
}

void VoiceCard::paint(juce::Graphics& g) {
    auto b = getLocalBounds().toFloat();
    g.setColour(panel);
    g.fillRoundedRectangle(b, (float) Spacing::radius);
    g.setColour(accentColour);
    g.fillRect(b.getX(), b.getY(), b.getWidth(), 3.0f);
    g.setColour(borderSoft);
    g.drawRoundedRectangle(b.reduced(0.5f), (float) Spacing::radius, 1.0f);

    auto header = getLocalBounds().reduced(16).removeFromTop(20);
    g.setColour(accentColour);
    g.fillEllipse(header.removeFromLeft(8).withSizeKeepingCentre(8, 8).toFloat());
    header.removeFromLeft(8);
    g.setColour(text);
    g.setFont(uiFont(12.0f, true));
    g.drawText("VOICE " + juce::String(index), header, juce::Justification::centredLeft);

    g.setColour(panelAlt);
    g.fillRoundedRectangle(readoutR.toFloat(), 10.0f);
    auto ro = readoutR.reduced(4);
    g.setColour(accentColour);
    g.setFont(monoFont(22.0f, true));
    g.drawText(buildReadoutMain(), ro.removeFromTop(ro.getHeight() * 0.6f), juce::Justification::centred);
    g.setColour(textFaint);
    g.setFont(uiFont(9.0f, true));
    g.drawText(buildReadoutCaption(), ro, juce::Justification::centred);

    g.setColour(textFaint);
    g.setFont(uiFont(10.0f, true));
    g.drawText("ENVELOPE", envelopeLabelR, juce::Justification::centredLeft);
    g.setFont(monoFont(9.0f));
    g.setColour(textDim);
    g.drawText(
        "A" + juce::String(juce::roundToInt(envelope.attackMsValue())) + "  D" + juce::String(juce::roundToInt(envelope.decayMsValue()))
        + "  S." + juce::String(juce::roundToInt(envelope.sustainValue() * 100)) + "  R" + juce::String(juce::roundToInt(envelope.releaseMsValue())),
        envelopeValuesR, juce::Justification::centred);

    g.setColour(textFaint);
    g.setFont(uiFont(10.0f, true));
    g.drawText("EQ", eqLabelR, juce::Justification::centredLeft);
    g.setFont(monoFont(9.0f));
    g.setColour(textDim);
    g.drawText(
        "LO" + juce::String(juce::roundToInt(eqCurve.lowDbValue())) + "  MID" + juce::String(juce::roundToInt(eqCurve.midDbValue()))
        + "  HI" + juce::String(juce::roundToInt(eqCurve.highDbValue())),
        eqValuesR, juce::Justification::centred);
}

} // namespace mv::ui
