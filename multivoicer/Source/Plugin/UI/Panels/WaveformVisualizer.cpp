#include "WaveformVisualizer.h"
#include <cmath>

namespace mv::ui {
using namespace Theme;

WaveformVisualizer::WaveformVisualizer(const std::atomic<float>& inputLevel,
                                        const std::array<std::atomic<float>, 4>& voiceLevels)
    : inputLevelRef(inputLevel), voiceLevelsRef(voiceLevels) {
    startTimerHz(30);
}

WaveformVisualizer::~WaveformVisualizer() { stopTimer(); }

void WaveformVisualizer::timerCallback() {
    phase += 0.05f;
    repaint();
}

void WaveformVisualizer::paint(juce::Graphics& g) {
    auto b = getLocalBounds().toFloat();
    const float midY = b.getCentreY();
    const float maxAmp = b.getHeight() * 0.42f;
    const int steps = 48;

    auto drawLine = [&](float freq1, float freq2, float speed, float phaseOffset,
                         float level, juce::Colour colour, float baseAlpha) {
        const float amp = juce::jmap(juce::jlimit(0.0f, 1.0f, level), 0.0f, 1.0f, maxAmp * 0.06f, maxAmp);
        const float ph = phase * speed + phaseOffset;

        juce::Path p;
        for (int i = 0; i <= steps; ++i) {
            const float t = (float) i / (float) steps;
            const float x = juce::jmap(t, 0.0f, 1.0f, b.getX(), b.getRight());
            const float wob = std::sin(t * freq1 + ph) + 0.5f * std::sin(t * freq2 * 1.7f - ph * 1.3f);
            const float y = midY - amp * wob * 0.5f;
            if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
        }
        g.setColour(colour.withAlpha(baseAlpha * 0.3f));
        g.strokePath(p, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved));
        g.setColour(colour.withAlpha(baseAlpha));
        g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved));
    };

    drawLine(9.0f, 15.0f, 0.6f, 0.0f, inputLevelRef.load(std::memory_order_relaxed) * 0.7f, brand, 0.25f);
    for (int i = 0; i < 4; ++i)
        drawLine(7.0f + (float) i * 1.3f, 12.0f - (float) i * 0.8f, 0.9f + (float) i * 0.25f, (float) i * 1.7f,
                  voiceLevelsRef[(size_t) i].load(std::memory_order_relaxed), voiceColour(i), 0.75f);
}

} // namespace mv::ui
