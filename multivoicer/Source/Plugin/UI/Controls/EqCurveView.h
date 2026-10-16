#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Theme.h"

namespace mv::ui {

// 3-band frequency-response curve for one voice. All three handles (low
// shelf, mid peak, high shelf) drag freely in both axes — x sets that band's
// corner/centre frequency, y sets its gain.
class EqCurveView : public juce::Component {
public:
    EqCurveView(juce::AudioProcessorValueTreeState& apvts, int voiceIndex, juce::Colour accent);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    float lowDbValue() const { return lowDb; }
    float midDbValue() const { return midDb; }
    float highDbValue() const { return highDb; }

private:
    enum class Handle { None, Low, Mid, High };

    float responseDb(float hz) const;
    float hzToX(float hz) const;
    float xToHz(float x) const;
    float dbToY(float db) const;
    float yToDb(float y) const;
    juce::Point<float> handlePos(Handle) const;

    juce::Colour accentColour;
    std::unique_ptr<juce::ParameterAttachment> lowDbAtt, lowHzAtt, midDbAtt, midHzAtt, midQAtt, highDbAtt, highHzAtt;
    float lowDb = 0.0f, lowHz = 200.0f, midDb = 0.0f, midHz = 1000.0f, midQ = 1.0f, highDb = 0.0f, highHz = 4000.0f;
    Handle dragging = Handle::None;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqCurveView)
};

} // namespace mv::ui
