#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class MultivoicerEditor : public juce::AudioProcessorEditor {
public:
    explicit MultivoicerEditor(MultivoicerProcessor& p);
    ~MultivoicerEditor() override = default;

    void resized() override;

private:
    juce::GenericAudioProcessorEditor generic;
};
