// Which Transport the bar listens on: ESP-NOW (default), or a wired serial link when built with
// STINGER_LINK_SERIAL (used by the Wokwi simulation, and ready for a wired installation).
#pragma once

#include <Arduino.h>

#include "transport.h"

#ifdef STINGER_LINK_SERIAL
#include "arduino_stream.h"
#include "serial_transport.h"
#else
#include "espnow_transport.h"
#endif

namespace stinger {

class BarLink {
  public:
    // Starts the link; false if the hardware could not be started.
    bool begin() {
#ifdef STINGER_LINK_SERIAL
        Serial.begin(STINGER_LINK_BAUD);
        uint64_t efuse = ESP.getEfuseMac(); // factory MAC, low byte first
        for (size_t i = 0; i < STINGER_MAC_SIZE; i++) {
            mac_[i] = static_cast<uint8_t>(efuse >> (8 * i));
        }
        return true;
#else
        if (!transport_.begin()) {
            return false;
        }
        memcpy(mac_, transport_.ownMac(), sizeof(mac_));
        return true;
#endif
    }

    Transport &transport() { return transport_; }
    const uint8_t *mac() const { return mac_; }

  private:
#ifdef STINGER_LINK_SERIAL
    ArduinoStream stream_{Serial};
    SerialTransport transport_{stream_};
#else
    EspNowTransport transport_{{STINGER_ESPNOW_CHANNEL, STINGER_ESPNOW_PHY_RATE,
                                STINGER_ESPNOW_ACK_TIMEOUT_MS, STINGER_ESPNOW_RX_QUEUE}};
#endif
    uint8_t mac_[STINGER_MAC_SIZE] = {0};
};

} // namespace stinger
