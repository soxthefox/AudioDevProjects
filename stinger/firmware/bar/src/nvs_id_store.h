// The bar ID in ESP32 non-volatile storage (survives power cycles and reflashing the app).
#pragma once

#include <Preferences.h>

#include "bar_logic.h"

namespace stinger {

class NvsIdStore : public IdStore {
  public:
    uint8_t load() override {
        prefs_.begin(kNamespace, /*readOnly=*/true);
        uint8_t id = prefs_.getUChar(kKey, STINGER_BAR_UNASSIGNED);
        prefs_.end();
        return id;
    }
    void save(uint8_t id) override {
        prefs_.begin(kNamespace, /*readOnly=*/false);
        prefs_.putUChar(kKey, id);
        prefs_.end();
    }

  private:
    static constexpr const char *kNamespace = "stinger";
    static constexpr const char *kKey = "bar_id";
    Preferences prefs_;
};

} // namespace stinger
