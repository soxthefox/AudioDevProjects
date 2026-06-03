#pragma once

#include "MusicTheory.h"

namespace mv::Scales {

// Major: 0 2 4 5 7 9 11 -> bits 0,2,4,5,7,9,11 set -> 0b0000'1010'1011'0101
inline constexpr ScaleDef Major {
    "Major",
    PitchClassSet{ 0b0000'1010'1011'0101 }
};

// Natural Minor: 0 2 3 5 7 8 10 -> bits 0,2,3,5,7,8,10 set -> 0b0000'0101'1010'1101
inline constexpr ScaleDef NaturalMinor {
    "Natural Minor",
    PitchClassSet{ 0b0000'0101'1010'1101 }
};

inline constexpr std::array<const ScaleDef*, 2> All { &Major, &NaturalMinor };

} // namespace mv::Scales
