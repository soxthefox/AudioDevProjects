// Bar firmware: packets from the gateway (ESP-NOW, or a wired serial link) -> WS2812B strip.
#include <Arduino.h>
#include <FastLED.h>

#include "bar_link.h"
#include "bar_logic.h"
#include "firmware_config.h"
#include "nvs_id_store.h"

namespace {

stinger::BarLink barLink;
stinger::NvsIdStore idStore;
stinger::BarLogic *bar = nullptr; // built in setup(), once the link knows the bar's MAC

CRGB leds[STINGER_LED_COUNT];
uint8_t rgb[3 * STINGER_LED_COUNT];
uint8_t lastShown[3 * STINGER_LED_COUNT];

const stinger::BarConfig kBarConfig = {STINGER_LED_COUNT,           STINGER_BRIGHTNESS_HARD_LIMIT,
                                       STINGER_FAILSAFE_TIMEOUT_MS, STINGER_FAILSAFE_FADE_MS,
                                       STINGER_HELLO_INTERVAL_MS,   STINGER_FW_VERSION};

} // namespace

void setup() {
    FastLED.addLeds<WS2812B, STINGER_DATA_PIN, GRB>(leds, STINGER_LED_COUNT);
    // Second guard after the firmware brightness cap (HANDOVER.md section 3).
    FastLED.setMaxPowerInVoltsAndMilliamps(STINGER_LED_VOLTS, STINGER_LED_MAX_MA);
    FastLED.setBrightness(255); // the cap is applied in BarLogic::render
    FastLED.clear(true);
    if (!barLink.begin()) {
        delay(1000);
        ESP.restart();
    }
    static stinger::BarLogic logic(barLink.transport(), idStore, barLink.mac(), kBarConfig);
    bar = &logic;
    bar->begin();
}

void loop() {
    uint32_t now = millis();
    bar->loop(now);
    bar->render(now, rgb);
    if (memcmp(rgb, lastShown, sizeof(rgb)) == 0) {
        return; // nothing changed: leave the strip alone
    }
    memcpy(lastShown, rgb, sizeof(rgb));
    for (size_t i = 0; i < STINGER_LED_COUNT; i++) {
        leds[i] = CRGB(rgb[3 * i], rgb[3 * i + 1], rgb[3 * i + 2]);
    }
    FastLED.show();
#ifdef STINGER_WOKWI_TRACE
    // Text the Wokwi scenario can wait for. It shares TX with the binary link, so this flag is
    // for simulation builds only.
    Serial.printf("\npixel0=%u,%u,%u\n", rgb[0], rgb[1], rgb[2]);
#endif
}
