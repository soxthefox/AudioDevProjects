#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>
#include "../Dsp/YinPitchDetector.h"
#include "../Voicing/VoiceManager.h"
#include "../Voicing/MidiVoiceAllocator.h"
#include "../Voicing/ModeRouter.h"
#include "Parameters.h"

namespace mv { class PresetBank; }

class MultivoicerProcessor : public juce::AudioProcessor {
public:
    MultivoicerProcessor();
    ~MultivoicerProcessor() override;

    const juce::String getName() const override { return "Multivoicer"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 6; }
    int getCurrentProgram() override;
    void setCurrentProgram(int idx) override;
    const juce::String getProgramName(int idx) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    mv::YinPitchDetector    detector;
    mv::VoiceManager        voices;
    mv::MidiVoiceAllocator  allocator;
    mv::ModeRouter          router;

    std::array<bool, 4> wasGated { false, false, false, false };
    mv::Mode             lastMode = mv::Mode::Preset;
    bool                 lastMonoMidi = false;
    int                  lastVoiceCount = 3;

    std::vector<float>   monoIn, wetL, wetR, dryL, dryR;

    void syncParamsToDsp();
    void loadPreset(int idx);
    int  currentPresetIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultivoicerProcessor)
};
