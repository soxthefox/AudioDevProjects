// Stinger packets over ESP-NOW (PROTOCOL.md sections 1 and 4.2). ESP32 only.
#pragma once

#ifdef ARDUINO_ARCH_ESP32

#include <esp_wifi_types.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "transport.h"

namespace stinger {

struct EspNowConfig {
    uint8_t channel;         // Wi-Fi channel; gateway and bars must match
    wifi_phy_rate_t phyRate; // bit rate of ESP-NOW frames
    uint32_t ackTimeoutMs;   // how long a unicast send waits for the MAC-layer ack
    uint8_t rxQueueLength;   // packets buffered between the radio task and poll()
};

class EspNowTransport : public Transport {
  public:
    explicit EspNowTransport(const EspNowConfig &config);

    // Starts Wi-Fi in station mode on the configured channel and initialises ESP-NOW.
    bool begin();
    const uint8_t *ownMac() const { return ownMac_; }

    bool send(const Address &to, const uint8_t *packet, size_t len) override;
    void onReceive(ReceiveHandler handler, void *ctx) override;
    void poll() override;
    LinkCounters counters() const override { return counters_; }

    // Entry points for the ESP-NOW callbacks (Wi-Fi task); not for application code.
    static void onRecvStatic(const uint8_t *mac, int8_t rssi, const uint8_t *data, int len);
    static void onSentStatic(const uint8_t *mac, bool delivered);

  private:
    struct RxItem {
        uint8_t mac[STINGER_MAC_SIZE];
        int8_t rssi;
        uint8_t len;
        uint8_t data[STINGER_MAX_PACKET];
    };

    bool ensurePeer(const Address &to);

    static EspNowTransport *instance_; // ESP-NOW callbacks carry no context pointer

    EspNowConfig config_;
    uint8_t ownMac_[STINGER_MAC_SIZE] = {0};
    QueueHandle_t rxQueue_ = nullptr;
    SemaphoreHandle_t ackDone_ = nullptr;
    Address awaitingAck_ = Address::broadcast();
    volatile bool waiting_ = false;
    volatile bool delivered_ = false;
    ReceiveHandler handler_ = nullptr;
    void *ctx_ = nullptr;
    LinkCounters counters_ = {0, 0};
};

} // namespace stinger

#endif // ARDUINO_ARCH_ESP32
