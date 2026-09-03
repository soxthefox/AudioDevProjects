#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../PluginProcessor.h"

namespace mv::ui {

// Bottom bar: live detected pitch and reported round-trip latency. Reads
// straight from the processor rather than caching, since nothing here is
// hot-path — a Timer poll at a few Hz is plenty.
class StatusFooter : public juce::Component, private juce::Timer {
public:
    explicit StatusFooter(MultivoicerProcessor& processor);
    ~StatusFooter() override;

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;

    MultivoicerProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusFooter)
};

} // namespace mv::ui
