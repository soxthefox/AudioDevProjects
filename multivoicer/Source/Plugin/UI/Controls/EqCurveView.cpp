#include "EqCurveView.h"
#include "../../Parameters.h"
#include <cmath>

namespace mv::ui {
using namespace Theme;

namespace {
constexpr float kMinHz = 30.0f;
constexpr float kMaxHz = 18000.0f;
constexpr float kDbRange = 18.0f;
constexpr float kLowHzMin = 40.0f, kLowHzMax = 800.0f;
constexpr float kMidHzMin = 200.0f, kMidHzMax = 8000.0f;
constexpr float kHighHzMin = 1000.0f, kHighHzMax = 12000.0f;
constexpr float kHandleR = 4.5f;
constexpr float kHitPad = 7.0f;
}

EqCurveView::EqCurveView(juce::AudioProcessorValueTreeState& apvts, int voiceIndex, juce::Colour accent)
    : accentColour(accent) {
    auto pid = [voiceIndex](const char* field) { return mv::Params::voiceParam(field, voiceIndex); };
    auto* lowDbP  = apvts.getParameter(pid("EqLowDb"));
    auto* lowHzP  = apvts.getParameter(pid("EqLowHz"));
    auto* midDbP  = apvts.getParameter(pid("EqMidDb"));
    auto* midHzP  = apvts.getParameter(pid("EqMidHz"));
    auto* midQP   = apvts.getParameter(pid("EqMidQ"));
    auto* highDbP = apvts.getParameter(pid("EqHighDb"));
    auto* highHzP = apvts.getParameter(pid("EqHighHz"));
    jassert(lowDbP && lowHzP && midDbP && midHzP && midQP && highDbP && highHzP);

    lowDbAtt  = std::make_unique<juce::ParameterAttachment>(*lowDbP,  [this](float v) { lowDb = v; repaint(); });
    lowHzAtt  = std::make_unique<juce::ParameterAttachment>(*lowHzP,  [this](float v) { lowHz = v; repaint(); });
    midDbAtt  = std::make_unique<juce::ParameterAttachment>(*midDbP,  [this](float v) { midDb = v; repaint(); });
    midHzAtt  = std::make_unique<juce::ParameterAttachment>(*midHzP,  [this](float v) { midHz = v; repaint(); });
    midQAtt   = std::make_unique<juce::ParameterAttachment>(*midQP,   [this](float v) { midQ = v; repaint(); });
    highDbAtt = std::make_unique<juce::ParameterAttachment>(*highDbP, [this](float v) { highDb = v; repaint(); });
    highHzAtt = std::make_unique<juce::ParameterAttachment>(*highHzP, [this](float v) { highHz = v; repaint(); });
    for (auto* att : { lowDbAtt.get(), lowHzAtt.get(), midDbAtt.get(), midHzAtt.get(), midQAtt.get(), highDbAtt.get(), highHzAtt.get() })
        att->sendInitialUpdate();
}

float EqCurveView::responseDb(float hz) const {
    const float lowFactor  = 1.0f / (1.0f + std::pow(hz / juce::jmax(20.0f, lowHz), 2.0f));
    const float highFactor = 1.0f / (1.0f + std::pow(juce::jmax(20.0f, highHz) / hz, 2.0f));
    const float logDist = std::log2(hz / juce::jmax(20.0f, midHz));
    const float bw = 1.0f / juce::jmax(0.15f, midQ);
    const float midFactor = std::exp(-(logDist * logDist) / (2.0f * bw * bw));
    return lowDb * lowFactor + midDb * midFactor + highDb * highFactor;
}

float EqCurveView::hzToX(float hz) const {
    auto b = getLocalBounds().toFloat();
    const float t = std::log(hz / kMinHz) / std::log(kMaxHz / kMinHz);
    return juce::jmap(juce::jlimit(0.0f, 1.0f, t), 0.0f, 1.0f, b.getX(), b.getRight());
}

float EqCurveView::xToHz(float x) const {
    auto b = getLocalBounds().toFloat();
    const float t = juce::jlimit(0.0f, 1.0f, juce::jmap(x, b.getX(), b.getRight(), 0.0f, 1.0f));
    return kMinHz * std::pow(kMaxHz / kMinHz, t);
}

float EqCurveView::dbToY(float db) const {
    auto b = getLocalBounds().toFloat();
    return juce::jmap(juce::jlimit(-kDbRange, kDbRange, db), -kDbRange, kDbRange, b.getBottom(), b.getY());
}

float EqCurveView::yToDb(float y) const {
    auto b = getLocalBounds().toFloat();
    return juce::jlimit(-kDbRange, kDbRange, juce::jmap(y, b.getBottom(), b.getY(), -kDbRange, kDbRange));
}

juce::Point<float> EqCurveView::handlePos(Handle h) const {
    switch (h) {
        case Handle::Low:  return { hzToX(lowHz), dbToY(lowDb) };
        case Handle::Mid:  return { hzToX(midHz), dbToY(midDb) };
        case Handle::High: return { hzToX(highHz), dbToY(highDb) };
        default:           return {};
    }
}

void EqCurveView::paint(juce::Graphics& g) {
    auto b = getLocalBounds().toFloat();
    const float zeroY = dbToY(0.0f);

    g.setColour(borderSoft.withAlpha(0.6f));
    g.drawHorizontalLine((int) zeroY, b.getX(), b.getRight());

    juce::Path curve;
    const int steps = 40;
    for (int i = 0; i <= steps; ++i) {
        const float t = (float) i / (float) steps;
        const float hz = kMinHz * std::pow(kMaxHz / kMinHz, t);
        const float x = juce::jmap(t, 0.0f, 1.0f, b.getX(), b.getRight());
        const float y = dbToY(responseDb(hz));
        if (i == 0) curve.startNewSubPath(x, y); else curve.lineTo(x, y);
    }

    auto fill = curve;
    fill.lineTo(b.getRight(), zeroY);
    fill.lineTo(b.getX(), zeroY);
    fill.closeSubPath();
    g.setColour(accentColour.withAlpha(0.16f));
    g.fillPath(fill);

    g.setColour(accentColour);
    g.strokePath(curve, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (auto h : { Handle::Low, Handle::Mid, Handle::High }) {
        auto p = handlePos(h);
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

void EqCurveView::mouseDown(const juce::MouseEvent& e) {
    dragging = Handle::None;
    float best = kHandleR + kHitPad;
    for (auto h : { Handle::Low, Handle::Mid, Handle::High }) {
        auto d = handlePos(h).getDistanceFrom(e.position);
        if (d <= best) { best = d; dragging = h; }
    }
    if (dragging == Handle::Low) { lowHzAtt->beginGesture(); lowDbAtt->beginGesture(); }
    else if (dragging == Handle::Mid) { midHzAtt->beginGesture(); midDbAtt->beginGesture(); }
    else if (dragging == Handle::High) { highHzAtt->beginGesture(); highDbAtt->beginGesture(); }
    repaint();
}

void EqCurveView::mouseDrag(const juce::MouseEvent& e) {
    if (dragging == Handle::None) return;

    if (dragging == Handle::Low) {
        lowHz = juce::jlimit(kLowHzMin, kLowHzMax, xToHz(e.position.x));
        lowHzAtt->setValueAsPartOfGesture(lowHz);
        lowDb = yToDb(e.position.y);
        lowDbAtt->setValueAsPartOfGesture(lowDb);
    } else if (dragging == Handle::Mid) {
        midHz = juce::jlimit(kMidHzMin, kMidHzMax, xToHz(e.position.x));
        midHzAtt->setValueAsPartOfGesture(midHz);
        midDb = yToDb(e.position.y);
        midDbAtt->setValueAsPartOfGesture(midDb);
    } else if (dragging == Handle::High) {
        highHz = juce::jlimit(kHighHzMin, kHighHzMax, xToHz(e.position.x));
        highHzAtt->setValueAsPartOfGesture(highHz);
        highDb = yToDb(e.position.y);
        highDbAtt->setValueAsPartOfGesture(highDb);
    }
    repaint();
}

void EqCurveView::mouseUp(const juce::MouseEvent&) {
    if (dragging == Handle::Low) { lowHzAtt->endGesture(); lowDbAtt->endGesture(); }
    else if (dragging == Handle::Mid) { midHzAtt->endGesture(); midDbAtt->endGesture(); }
    else if (dragging == Handle::High) { highHzAtt->endGesture(); highDbAtt->endGesture(); }
    dragging = Handle::None;
    repaint();
}

} // namespace mv::ui
