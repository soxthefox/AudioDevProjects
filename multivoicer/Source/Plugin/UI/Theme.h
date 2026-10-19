#pragma once

#include <juce_graphics/juce_graphics.h>

// Colour/type/spacing tokens shared by every UI component. Keep this the
// single source of truth so the panels never hardcode a colour directly.
namespace mv::ui::Theme {

inline const juce::Colour bg        { 0xFF15171E };
inline const juce::Colour panel     { 0xFF1D2029 };
inline const juce::Colour panelAlt  { 0xFF262A36 };
inline const juce::Colour border    { 0x552F3444 };
inline const juce::Colour borderSoft{ 0x3A2F3444 };
inline const juce::Colour text      { 0xFFEDEEF2 };
inline const juce::Colour textDim   { 0xFF9AA0AE };
inline const juce::Colour textFaint { 0xFF6B7080 };
inline const juce::Colour brand     { 0xFF7C6FF0 };
inline const juce::Colour brandInk  { 0xFF14101F };

inline const juce::Colour voiceColours[4] = {
    juce::Colour(0xFFE0A227), // v1 amber
    juce::Colour(0xFF3FC6C1), // v2 teal
    juce::Colour(0xFFE0529E), // v3 magenta
    juce::Colour(0xFF4FCB7A), // v4 green
};

inline juce::Colour voiceColour(int idx) { return voiceColours[juce::jlimit(0, 3, idx)]; }

inline juce::Font uiFont(float size, bool bold = false) {
    return juce::Font(juce::FontOptions{}.withHeight(size).withStyle(bold ? "Bold" : "Regular"));
}

inline juce::Font monoFont(float size, bool bold = false) {
    return juce::Font(juce::FontOptions{}
        .withName(juce::Font::getDefaultMonospacedFontName())
        .withHeight(size)
        .withStyle(bold ? "Bold" : "Regular"));
}

namespace Spacing {
    inline constexpr int gap    = 16;
    inline constexpr int pad    = 16;
    inline constexpr int radius = 12;
}

} // namespace mv::ui::Theme
