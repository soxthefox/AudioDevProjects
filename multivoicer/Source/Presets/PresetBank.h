#pragma once

#include <vector>
#include "Preset.h"

namespace mv {

class PresetBank {
public:
    static std::vector<Preset> defaults();
};

} // namespace mv
