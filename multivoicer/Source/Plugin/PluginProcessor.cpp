#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../Presets/PresetBank.h"
#include "../Music/Scales.h"
#include <algorithm>
#include <cmath>

using mv::Mode;
using mv::PresetTrigger;

MultivoicerProcessor::MultivoicerProcessor()
    : juce::AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "MultivoicerState", mv::Params::buildLayout()) {}

MultivoicerProcessor::~MultivoicerProcessor() = default;

void MultivoicerProcessor::prepareToPlay(double sr, int maxBlock) {
    detector.prepare(sr, maxBlock);
    voices.prepare(sr, maxBlock);
    allocator.setPoly(4);
    monoIn.assign((size_t)maxBlock, 0.0f);
    wetL.assign((size_t)maxBlock, 0.0f);
    wetR.assign((size_t)maxBlock, 0.0f);
    dryL.assign((size_t)maxBlock, 0.0f);
    dryR.assign((size_t)maxBlock, 0.0f);
}

void MultivoicerProcessor::syncParamsToDsp() {
    int voiceCount = (int) *apvts.getRawParameterValue(mv::Params::kVoiceCount);
    bool monoMidi = (bool) *apvts.getRawParameterValue(mv::Params::kMonoMidiMode);
    Mode mode = (int) *apvts.getRawParameterValue(mv::Params::kMode) == 0 ? Mode::Preset : Mode::Midi;

    if (voiceCount != lastVoiceCount) {
        voices.setNumActiveVoices(voiceCount);
        lastVoiceCount = voiceCount;
    }
    if (mode != lastMode || monoMidi != lastMonoMidi) {
        for (int i = 0; i < 4; ++i) { voices.voice(i).noteOff(); wasGated[i] = false; }
        allocator.clearAll();
        if (mode == Mode::Midi) {
            if (monoMidi || voiceCount == 1) allocator.setMono();
            else                              allocator.setPoly(voiceCount);
        }
        lastMode = mode;
        lastMonoMidi = monoMidi;
    }
    for (int i = 0; i < 4; ++i) {
        mv::VoiceParams p {};
        const int idx = i + 1;
        p.enabled    = (bool) *apvts.getRawParameterValue(mv::Params::voiceParam("Enabled", idx));
        p.interval   = (int)  *apvts.getRawParameterValue(mv::Params::voiceParam("Interval", idx));
        p.gainDb     = *apvts.getRawParameterValue(mv::Params::voiceParam("GainDb", idx));
        p.pan        = *apvts.getRawParameterValue(mv::Params::voiceParam("Pan", idx));
        p.attack     = *apvts.getRawParameterValue(mv::Params::voiceParam("AttackMs", idx));
        p.decay      = *apvts.getRawParameterValue(mv::Params::voiceParam("DecayMs", idx));
        p.sustain    = *apvts.getRawParameterValue(mv::Params::voiceParam("Sustain", idx));
        p.release    = *apvts.getRawParameterValue(mv::Params::voiceParam("ReleaseMs", idx));
        p.lowShelfDb = *apvts.getRawParameterValue(mv::Params::voiceParam("EqLowDb", idx));
        p.midPeakDb  = *apvts.getRawParameterValue(mv::Params::voiceParam("EqMidDb", idx));
        p.midFreqHz  = *apvts.getRawParameterValue(mv::Params::voiceParam("EqMidHz", idx));
        p.midQ       = *apvts.getRawParameterValue(mv::Params::voiceParam("EqMidQ", idx));
        p.highShelfDb= *apvts.getRawParameterValue(mv::Params::voiceParam("EqHighDb", idx));
        voices.voice(i).setParams(p);
    }
}

void MultivoicerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals _;
    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();

    syncParamsToDsp();

    const float* inL = buffer.getReadPointer(0);
    const float* inR = nCh > 1 ? buffer.getReadPointer(1) : buffer.getReadPointer(0);
    for (int i = 0; i < n; ++i) {
        dryL[i] = inL[i];
        dryR[i] = inR[i];
        monoIn[i] = 0.5f * (inL[i] + inR[i]);
        wetL[i] = 0.0f; wetR[i] = 0.0f;
    }

    auto det = detector.process(monoIn.data(), n);

    for (const auto meta : midi) {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())  allocator.noteOn(msg.getNoteNumber());
        if (msg.isNoteOff()) allocator.noteOff(msg.getNoteNumber());
    }
    allocator.tick();

    float rms = 0.0f;
    for (int i = 0; i < n; ++i) rms += monoIn[i] * monoIn[i];
    rms = std::sqrt(rms / std::max(1, n));

    mv::ModeRouter::Inputs ri {};
    ri.mode = lastMode;
    int triggerIdx = (int) *apvts.getRawParameterValue(mv::Params::kPresetTrigger);
    ri.trigger = triggerIdx == 0 ? PresetTrigger::InputGate
              : triggerIdx == 1 ? PresetTrigger::PitchGate : PresetTrigger::AlwaysOn;
    ri.inputGateThresholdDb = *apvts.getRawParameterValue(mv::Params::kInputGateThresholdDb);
    ri.pitchConfidenceMin   = *apvts.getRawParameterValue(mv::Params::kPitchConfidenceMin);
    int rootIdx  = (int) *apvts.getRawParameterValue(mv::Params::kKeyRoot);
    int scaleIdx = (int) *apvts.getRawParameterValue(mv::Params::kKeyScale);
    ri.key.root  = (mv::NoteName) rootIdx;
    ri.key.scale = mv::Scales::All[std::clamp(scaleIdx, 0, (int) mv::Scales::All.size() - 1)];
    ri.numActiveVoices = lastVoiceCount;
    for (int i = 0; i < 4; ++i)
        ri.voiceIntervals[i] = (int) *apvts.getRawParameterValue(mv::Params::voiceParam("Interval", i + 1));
    ri.inputFreqHz = det.freqHz;
    ri.voiced = det.voiced && det.confidence >= ri.pitchConfidenceMin;
    ri.dryRMS = rms;

    auto decisions = router.decide(ri, allocator.snapshot());

    for (int i = 0; i < 4; ++i) {
        if (decisions[i].targetFreqHz > 0.0)
            voices.voice(i).setTargetFreq(decisions[i].targetFreqHz);
        if (decisions[i].gateOn && !wasGated[i]) voices.voice(i).noteOn();
        if (!decisions[i].gateOn && wasGated[i]) voices.voice(i).noteOff();
        wasGated[i] = decisions[i].gateOn;
    }

    voices.renderAdd(monoIn.data(), wetL.data(), wetR.data(), n, det.voiced ? det.freqHz : 110.0);

    const float mix = *apvts.getRawParameterValue(mv::Params::kDryWetMix);
    float* outL = buffer.getWritePointer(0);
    float* outR = nCh > 1 ? buffer.getWritePointer(1) : buffer.getWritePointer(0);
    for (int i = 0; i < n; ++i) {
        outL[i] = (1.0f - mix) * dryL[i] + mix * wetL[i];
        outR[i] = (1.0f - mix) * dryR[i] + mix * wetR[i];
    }
}

int  MultivoicerProcessor::getCurrentProgram() { return currentPresetIndex; }
void MultivoicerProcessor::setCurrentProgram(int idx) {
    if (idx < 0) idx = 0;
    auto bank = mv::PresetBank::defaults();
    if (idx >= (int) bank.size()) idx = (int) bank.size() - 1;
    currentPresetIndex = idx;
    loadPreset(idx);
}
const juce::String MultivoicerProcessor::getProgramName(int idx) {
    auto bank = mv::PresetBank::defaults();
    if (idx >= 0 && idx < (int) bank.size()) return bank[idx].name;
    return {};
}

void MultivoicerProcessor::loadPreset(int idx) {
    auto bank = mv::PresetBank::defaults();
    if (idx < 0 || idx >= (int) bank.size()) return;
    const auto& p = bank[idx];

    auto setF = [&](const juce::String& id, float v) {
        if (auto* param = apvts.getParameter(id)) param->setValueNotifyingHost(
            apvts.getParameterRange(id).convertTo0to1(v));
    };
    auto setI = [&](const juce::String& id, int v) { setF(id, (float)v); };
    auto setB = [&](const juce::String& id, bool v) { setF(id, v ? 1.0f : 0.0f); };

    setI(mv::Params::kMode, p.mode == Mode::Preset ? 0 : 1);
    setI(mv::Params::kVoiceCount, p.numActiveVoices);
    setB(mv::Params::kMonoMidiMode, p.monoMidiMode);
    setB(mv::Params::kFormantPreserve, p.formantPreserve);
    setF(mv::Params::kDryWetMix, p.dryWetMix);
    setI(mv::Params::kPresetTrigger, (int) p.trigger);
    setF(mv::Params::kInputGateThresholdDb, p.inputGateThresholdDb);
    setF(mv::Params::kInputGateHoldMs, p.inputGateHoldMs);
    setF(mv::Params::kPitchConfidenceMin, p.pitchConfidenceMin);
    for (int i = 0; i < 4; ++i) {
        const auto& v = p.voices[i];
        const int idx1 = i + 1;
        setB(mv::Params::voiceParam("Enabled", idx1), v.enabled);
        setI(mv::Params::voiceParam("Interval", idx1), v.interval);
        setF(mv::Params::voiceParam("GainDb", idx1), v.gainDb);
        setF(mv::Params::voiceParam("Pan", idx1), v.pan);
        setF(mv::Params::voiceParam("AttackMs", idx1), v.attack);
        setF(mv::Params::voiceParam("DecayMs", idx1), v.decay);
        setF(mv::Params::voiceParam("Sustain", idx1), v.sustain);
        setF(mv::Params::voiceParam("ReleaseMs", idx1), v.release);
        setF(mv::Params::voiceParam("EqLowDb", idx1), v.lowShelfDb);
        setF(mv::Params::voiceParam("EqMidDb", idx1), v.midPeakDb);
        setF(mv::Params::voiceParam("EqMidHz", idx1), v.midFreqHz);
        setF(mv::Params::voiceParam("EqMidQ", idx1), v.midQ);
        setF(mv::Params::voiceParam("EqHighDb", idx1), v.highShelfDb);
    }
}

void MultivoicerProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}

void MultivoicerProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* MultivoicerProcessor::createEditor() {
    return new MultivoicerEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new MultivoicerProcessor();
}
