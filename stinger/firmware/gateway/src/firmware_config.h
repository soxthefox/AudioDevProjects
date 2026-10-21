// Every hardware and timing fact comes from build flags in platformio.ini, which must agree with
// config/installation.yaml (tests/test_config.py checks). No defaults here, so a missing flag
// fails the build instead of silently using a stale number.
#pragma once

#ifndef STINGER_SERIAL_BAUD
#error "STINGER_SERIAL_BAUD must be set in platformio.ini"
#endif
#ifndef STINGER_SERIAL_RX_BUFFER
#error "STINGER_SERIAL_RX_BUFFER must be set in platformio.ini"
#endif
#ifndef STINGER_STATS_INTERVAL_MS
#error "STINGER_STATS_INTERVAL_MS must be set in platformio.ini"
#endif
#ifndef STINGER_CONFIG_RETRIES
#error "STINGER_CONFIG_RETRIES must be set in platformio.ini"
#endif
#ifndef STINGER_OFFLINE_AFTER_MS
#error "STINGER_OFFLINE_AFTER_MS must be set in platformio.ini"
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
