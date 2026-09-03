#include "EnvelopeGraph.h"
#include "../../Parameters.h"
#include <cmath>

namespace mv::ui {
using namespace Theme;

namespace {
constexpr float kMaxAttackMs = 2000.0f;
constexpr float kMaxDecayMs = 2000.0f;
constexpr float kMaxReleaseMs = 5000.0f;
constexpr float kHandleR = 4.5f;
constexpr float kHitPad = 7.0f;

float timeToNorm(float ms, float maxMs) { return std::sqrt(juce::jlimit(0.0f, maxMs, ms) / maxMs); }
float normToTime(float norm, float maxMs) { norm = juce::jlimit(0.0f, 1.0f, norm); return norm * norm * maxMs; }
}

EnvelopeGraph::EnvelopeGraph(juce::AudioProcessorValueTreeState& apvts, int voiceIndex, juce::Colour accent)
    : accentColour(accent) {
    auto pid = [voiceIndex](const char* field) { return mv::Params::voiceParam(field, voiceIndex); };
    auto* aP  = apvts.getParameter(pid("AttackMs"));
    auto* alP = apvts.getParameter(pid("AttackLevel"));
    auto* dP  = apvts.getParameter(pid("DecayMs"));
    auto* sP  = apvts.getParameter(pid("Sustain"));
    auto* rP  = apvts.getParameter(pid("ReleaseMs"));
    auto* rlP = apvts.getParameter(pid("ReleaseLevel"));
    jassert(aP && alP && dP && sP && rP && rlP);

    attackAtt      = std::make_unique<juce::ParameterAttachment>(*aP,  [this](float v) { attackMs = v; repaint(); });
    attackLevelAtt = std::make_unique<juce::ParameterAttachment>(*alP, [this](float v) { attackLevel = v; repaint(); });
    decayAtt       = std::make_unique<juce::ParameterAttachment>(*dP,  [this](float v) { decayMs = v; repaint(); });
    sustainAtt     = std::make_unique<juce::ParameterAttachment>(*sP,  [this](float v) { sustain = v; repaint(); });
    releaseAtt     = std::make_unique<juce::ParameterAttachment>(*rP,  [this](float v) { releaseMs = v; repaint(); });
    releaseLevelAtt= std::make_unique<juce::ParameterAttachment>(*rlP, [this](float v) { releaseLevel = v; repaint(); });
    for (auto* att : { attackAtt.get(), attackLevelAtt.get(), decayAtt.get(), sustainAtt.get(), releaseAtt.get(), releaseLevelAtt.get() })
        att->sendInitialUpdate();
}

EnvelopeGraph::Layout EnvelopeGraph::computeLayout() const {
    auto b = getLocalBounds().toFloat().reduced(6.0f, 6.0f);
    Layout L{};
    L.x0 = b.getX();
    L.right = b.getRight();
    L.peakY = b.getY();
    L.baselineY = b.getBottom();

    auto levelToY = [&](float lvl) { return L.peakY + (1.0f - lvl) * (L.baselineY - L.peakY); };
    L.attackX  = juce::jmap(timeToNorm(attackMs, kMaxAttackMs), 0.0f, 1.0f, L.x0, L.right);
    L.attackY  = levelToY(attackLevel);
    L.decayX   = juce::jmap(timeToNorm(decayMs, kMaxDecayMs), 0.0f, 1.0f, L.x0, L.right);
    L.decayY   = levelToY(sustain);
    L.releaseX = juce::jmap(timeToNorm(releaseMs, kMaxReleaseMs), 0.0f, 1.0f, L.x0, L.right);
    L.releaseY = levelToY(releaseLevel);
    return L;
}

juce::Point<float> EnvelopeGraph::handlePos(Handle h, const Layout& L) const {
    switch (h) {
        case Handle::Attack:  return { L.attackX, L.attackY };
        case Handle::Decay:   return { L.decayX, L.decayY };
        case Handle::Release: return { L.releaseX, L.releaseY };
        default:              return {};
    }
}

float EnvelopeGraph::xToMs(float x, const Layout& L, float maxMs) const {
    return normToTime(juce::jmap(x, L.x0, L.right, 0.0f, 1.0f), maxMs);
}

float EnvelopeGraph::yToLevel(float y, const Layout& L) const {
    return juce::jlimit(0.0f, 1.0f, juce::jmap(y, L.peakY, L.baselineY, 1.0f, 0.0f));
}

void EnvelopeGraph::paint(juce::Graphics& g) {
    const auto L = computeLayout();

    g.setColour(borderSoft.withAlpha(0.5f));
    for (float f : { 0.25f, 0.5f, 0.75f })
        g.drawHorizontalLine((int) juce::jmap(f, L.peakY, L.baselineY), L.x0, L.right);
    for (float f : { 0.25f, 0.5f, 0.75f })
        g.drawVerticalLine((int) juce::jmap(f, L.x0, L.right), L.peakY, L.baselineY);

    juce::Path curve;
    curve.startNewSubPath(L.x0, L.baselineY);
    curve.lineTo(L.attackX, L.attackY);
    curve.lineTo(L.decayX, L.decayY);
    curve.lineTo(L.releaseX, L.releaseY);
    curve.lineTo(L.right, L.releaseY);

    auto fill = curve;
    fill.lineTo(L.right, L.baselineY);
    fill.lineTo(L.x0, L.baselineY);
    fill.closeSubPath();
    g.setColour(accentColour.withAlpha(0.10f));
    g.fillPath(fill);

    g.setColour(accentColour);
    g.strokePath(curve, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (auto h : { Handle::Attack, Handle::Decay, Handle::Release }) {
        auto p = handlePos(h, L);
        const bool active = (h == dragging);
        if (active) {
            g.setColour(accentColour.withAlpha(0.35f));
            g.drawEllipse(p.x - kHandleR * 2.0f, p.y - kHandleR * 2.0f, kHandleR * 4.0f, kHandleR * 4.0f, 1.0f);
            g.setColour(accentColour);
            g.fillEllipse(p.x - (kHandleR + 1.5f), p.y - (kHandleR + 1.5f), (kHandleR + 1.5f) * 2.0f, (kHandleR + 1.5f) * 2.0f);
        } else {
            g.setColour(bg);
            g.fillEllipse(p.x - kHandleR, p.y - kHandleR, kHandleR * 2.0f, kHandleR * 2.0f);
            g.setColour(accentColour);
            g.drawEllipse(p.x - kHandleR, p.y - kHandleR, kHandleR * 2.0f, kHandleR * 2.0f, 2.0f);
        }
    }
}

void EnvelopeGraph::mouseDown(const juce::MouseEvent& e) {
    const auto L = computeLayout();
    dragging = Handle::None;
    float best = kHandleR + kHitPad;
    for (auto h : { Handle::Attack, Handle::Decay, Handle::Release }) {
        auto d = handlePos(h, L).getDistanceFrom(e.position);
        if (d <= best) { best = d; dragging = h; }
    }
    if (dragging == Handle::Attack) { attackAtt->beginGesture(); attackLevelAtt->beginGesture(); }
    else if (dragging == Handle::Decay) { decayAtt->beginGesture(); sustainAtt->beginGesture(); }
    else if (dragging == Handle::Release) { releaseAtt->beginGesture(); releaseLevelAtt->beginGesture(); }
    repaint();
}

void EnvelopeGraph::mouseDrag(const juce::MouseEvent& e) {
    if (dragging == Handle::None) return;
    const auto L = computeLayout();

    if (dragging == Handle::Attack) {
        attackMs = xToMs(e.position.x, L, kMaxAttackMs);
        attackAtt->setValueAsPartOfGesture(attackMs);
        attackLevel = yToLevel(e.position.y, L);
        attackLevelAtt->setValueAsPartOfGesture(attackLevel);
    } else if (dragging == Handle::Decay) {
        decayMs = xToMs(e.position.x, L, kMaxDecayMs);
        decayAtt->setValueAsPartOfGesture(decayMs);
        sustain = yToLevel(e.position.y, L);
        sustainAtt->setValueAsPartOfGesture(sustain);
    } else if (dragging == Handle::Release) {
        releaseMs = xToMs(e.position.x, L, kMaxReleaseMs);
        releaseAtt->setValueAsPartOfGesture(releaseMs);
        releaseLevel = yToLevel(e.position.y, L);
        releaseLevelAtt->setValueAsPartOfGesture(releaseLevel);
    }
    repaint();
}

void EnvelopeGraph::mouseUp(const juce::MouseEvent&) {
    if (dragging == Handle::Attack) { attackAtt->endGesture(); attackLevelAtt->endGesture(); }
    else if (dragging == Handle::Decay) { decayAtt->endGesture(); sustainAtt->endGesture(); }
    else if (dragging == Handle::Release) { releaseAtt->endGesture(); releaseLevelAtt->endGesture(); }
    dragging = Handle::None;
    repaint();
}

} // namespace mv::ui
