#pragma once

#include <array>
#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/MultivoicerLookAndFeel.h"
#include "UI/Panels/HeroHeader.h"
#include "UI/Panels/GlobalControlStrip.h"
#include "UI/Panels/TriggerRow.h"
#include "UI/Panels/MidiStatusRow.h"
#include "UI/Panels/VoiceCard.h"
#include "UI/Panels/StatusFooter.h"

class MultivoicerEditor : public juce::AudioProcessorEditor {
public:
    explicit MultivoicerEditor(MultivoicerProcessor& p);
    ~MultivoicerEditor() override;

    void resized() override;

private:
    mv::ui::MultivoicerLookAndFeel lookAndFeel;

    mv::ui::HeroHeader header;
    mv::ui::GlobalControlStrip globalStrip;
    mv::ui::TriggerRow triggerRow;
    mv::ui::MidiStatusRow midiRow;
    std::array<std::unique_ptr<mv::ui::VoiceCard>, 4> voiceCards;
    mv::ui::StatusFooter footer;

    std::unique_ptr<juce::ParameterAttachment> modeAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultivoicerEditor)
};
