#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Theme.h"

namespace mv::ui {

// Free-form XY breakpoint editor for one voice's envelope: attack, decay and
// release each drag in both axes (x = that stage's time, y = the level it
// ramps to) — no fixed lanes or clamped ranges, so the three points shape
// whatever curve you want. Backed by mv::Envelope, not juce::ADSR.
class EnvelopeGraph : public juce::Component {
public:
    EnvelopeGraph(juce::AudioProcessorValueTreeState& apvts, int voiceIndex, juce::Colour accent);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    float attackMsValue() const { return attackMs; }
    float decayMsValue() const { return decayMs; }
    float sustainValue() const { return sustain; }
    float releaseMsValue() const { return releaseMs; }

private:
    enum class Handle { None, Attack, Decay, Release };

    struct Layout {
        float x0, right, baselineY, peakY;
        float attackX, attackY, decayX, decayY, releaseX, releaseY;
    };
    Layout computeLayout() const;
    juce::Point<float> handlePos(Handle, const Layout&) const;
    float xToMs(float x, const Layout&, float maxMs) const;
    float yToLevel(float y, const Layout&) const;

    juce::Colour accentColour;
    std::unique_ptr<juce::ParameterAttachment> attackAtt, attackLevelAtt, decayAtt, sustainAtt, releaseAtt, releaseLevelAtt;
    float attackMs = 20.0f, attackLevel = 1.0f;
    float decayMs = 80.0f, sustain = 0.8f;
    float releaseMs = 300.0f, releaseLevel = 0.0f;
    Handle dragging = Handle::None;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeGraph)
};

} // namespace mv::ui
