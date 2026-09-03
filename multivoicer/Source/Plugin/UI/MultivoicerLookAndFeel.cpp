#include "MultivoicerLookAndFeel.h"
#include "Theme.h"

namespace mv::ui {
using namespace Theme;

MultivoicerLookAndFeel::MultivoicerLookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, bg);
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, text);
}

void MultivoicerLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                               float pos, float startAngle, float endAngle,
                                               juce::Slider& slider) {
    auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(3.0f);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto centre = bounds.getCentre();
    const float thickness = juce::jmax(2.5f, radius * 0.22f);
    const float arcRadius = radius - thickness * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId));
    g.strokePath(track, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
    g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
    g.strokePath(value, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void MultivoicerLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
                                               float sliderPos, float, float,
                                               juce::Slider::SliderStyle, juce::Slider& slider) {
    auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h);
    const float cy = bounds.getCentreY();
    const float trackH = 5.0f;

    g.setColour(slider.findColour(juce::Slider::backgroundColourId));
    g.fillRoundedRectangle(bounds.getX(), cy - trackH * 0.5f, bounds.getWidth(), trackH, trackH * 0.5f);

    const float centreX = bounds.getCentreX();
    g.setColour(slider.findColour(juce::Slider::thumbColourId).withAlpha(0.85f));
    g.fillRect(centreX - 1.0f, cy - 6.0f, 2.0f, 12.0f);

    const float thumbR = 5.0f;
    g.setColour(slider.findColour(juce::Slider::thumbColourId));
    g.fillEllipse(sliderPos - thumbR, cy - thumbR, thumbR * 2.0f, thumbR * 2.0f);
}

void MultivoicerLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                               bool, bool) {
    auto bounds = button.getLocalBounds().toFloat();
    const bool on = button.getToggleState();
    const auto accent = button.findColour(juce::ToggleButton::tickColourId);

    g.setColour(on ? accent.withAlpha(0.22f) : panelAlt);
    g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.5f);
    if (!on) {
        g.setColour(borderSoft);
        g.drawRoundedRectangle(bounds.reduced(0.5f), bounds.getHeight() * 0.5f, 1.0f);
    }

    const float d = bounds.getHeight() - 4.0f;
    const float thumbX = on ? bounds.getRight() - d - 2.0f : bounds.getX() + 2.0f;
    g.setColour(on ? accent : textFaint);
    g.fillEllipse(thumbX, bounds.getY() + 2.0f, d, d);
}

void MultivoicerLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                           int buttonX, int buttonY, int buttonW, int buttonH,
                                           juce::ComboBox& box) {
    auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height);
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

    auto arrowArea = juce::Rectangle<float>((float) buttonX, (float) buttonY, (float) buttonW, (float) buttonH);
    auto c = arrowArea.getCentre();
    juce::Path arrow;
    arrow.startNewSubPath(c.x - 4.0f, c.y - 2.0f);
    arrow.lineTo(c.x, c.y + 2.5f);
    arrow.lineTo(c.x + 4.0f, c.y - 2.0f);
    g.setColour(textDim);
    g.strokePath(arrow, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font MultivoicerLookAndFeel::getComboBoxFont(juce::ComboBox&) { return uiFont(13.0f, true); }
juce::Font MultivoicerLookAndFeel::getPopupMenuFont() { return uiFont(13.0f, true); }

void MultivoicerLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height) {
    g.setColour(panel);
    g.fillRect(0, 0, width, height);
    g.setColour(borderSoft);
    g.drawRect(0, 0, width, height, 1);
}

void MultivoicerLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                                bool isSeparator, bool, bool isHighlighted, bool isTicked,
                                                bool, const juce::String& itemText, const juce::String&,
                                                const juce::Drawable*, const juce::Colour*) {
    if (isSeparator) {
        g.setColour(borderSoft);
        g.drawLine((float) area.getX() + 8, (float) area.getCentreY(), (float) area.getRight() - 8, (float) area.getCentreY());
        return;
    }

    auto r = area.toFloat();
    if (isHighlighted) {
        g.setColour(brand.withAlpha(0.16f));
        g.fillRoundedRectangle(r.reduced(3.0f, 1.0f), 6.0f);
    }
    g.setColour(isTicked ? brand : text);
    g.setFont(uiFont(13.0f, isTicked));
    g.drawText(itemText, area.reduced(14, 0), juce::Justification::centredLeft);
}

} // namespace mv::ui
