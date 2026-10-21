// Gateway firmware: USB serial from the laptop <-> ESP-NOW to the bars.
#include <Arduino.h>

#include "arduino_stream.h"
#include "espnow_transport.h"
#include "firmware_config.h"
#include "gateway_logic.h"
#include "serial_transport.h"

namespace {

stinger::ArduinoStream usb(Serial);
stinger::SerialTransport host(usb);
stinger::EspNowTransport radio({STINGER_ESPNOW_CHANNEL, STINGER_ESPNOW_PHY_RATE,
                                STINGER_ESPNOW_ACK_TIMEOUT_MS, STINGER_ESPNOW_RX_QUEUE});
stinger::GatewayLogic gateway(host, radio,
                              {STINGER_CONFIG_RETRIES, STINGER_STATS_INTERVAL_MS,
                               STINGER_OFFLINE_AFTER_MS});

} // namespace

void setup() {
    Serial.setRxBufferSize(STINGER_SERIAL_RX_BUFFER);
    Serial.begin(STINGER_SERIAL_BAUD);
    if (!radio.begin()) {
        // Without the radio the gateway is useless; restart and try again.
        delay(1000);
        ESP.restart();
    }
    gateway.begin();
}

void loop() {
    gateway.loop(millis());
}
