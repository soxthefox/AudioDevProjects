// Every hardware and timing fact comes from build flags in platformio.ini, which must agree with
// config/installation.yaml (tests/test_config.py checks). No defaults here, so a missing flag
// fails the build instead of silently using a stale number.
#pragma once

#include "stinger_proto.h"

#ifndef STINGER_LED_COUNT
#error "STINGER_LED_COUNT must be set in platformio.ini"
#endif
#ifndef STINGER_DATA_PIN
#error "STINGER_DATA_PIN must be set in platformio.ini"
#endif
#ifndef STINGER_BRIGHTNESS_HARD_LIMIT
#error "STINGER_BRIGHTNESS_HARD_LIMIT must be set in platformio.ini"
#endif
#ifndef STINGER_LED_VOLTS
#error "STINGER_LED_VOLTS must be set in platformio.ini"
#endif
#ifndef STINGER_LED_MAX_MA
#error "STINGER_LED_MAX_MA must be set in platformio.ini"
#endif
#ifndef STINGER_FAILSAFE_TIMEOUT_MS
#error "STINGER_FAILSAFE_TIMEOUT_MS must be set in platformio.ini"
#endif
#ifndef STINGER_FAILSAFE_FADE_MS
#error "STINGER_FAILSAFE_FADE_MS must be set in platformio.ini"
#endif
#ifndef STINGER_HELLO_INTERVAL_MS
#error "STINGER_HELLO_INTERVAL_MS must be set in platformio.ini"
#endif
#ifndef STINGER_FW_VERSION
#error "STINGER_FW_VERSION must be set in platformio.ini"
#endif
#ifndef STINGER_ESPNOW_CHANNEL
#error "STINGER_ESPNOW_CHANNEL must be set in platformio.ini"
#endif
#ifndef STINGER_ESPNOW_PHY_RATE
#error "STINGER_ESPNOW_PHY_RATE must be set in platformio.ini"
#endif
#ifndef STINGER_ESPNOW_ACK_TIMEOUT_MS
#error "STINGER_ESPNOW_ACK_TIMEOUT_MS must be set in platformio.ini"
#endif
#ifndef STINGER_ESPNOW_RX_QUEUE
#error "STINGER_ESPNOW_RX_QUEUE must be set in platformio.ini"
#endif

static_assert(STINGER_LED_COUNT <= STINGER_MAX_FRAME_LEDS,
              "one FRAME carries at most 81 LEDs (DECISIONS.md D5)");
