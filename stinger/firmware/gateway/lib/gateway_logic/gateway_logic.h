// Gateway rules (PROTOCOL.md section 5): host link <-> radio link. No Arduino dependencies.
#pragma once

#include "transport.h"

namespace stinger {

struct GatewayConfig {
    uint8_t configRetries;    // unicast CONFIG retries after the first attempt
    uint32_t statsIntervalMs; // STATS period
    uint32_t offlineAfterMs;  // CONFIG to all bars reaches bars heard within this time
};

class GatewayLogic {
  public:
    GatewayLogic(Transport &host, Transport &radio, const GatewayConfig &config);

    void begin();
    // Call from the main loop: receives on both links, then sends STATS when due.
    void loop(uint32_t nowMs);

    uint32_t txFailures() const { return txFailures_; }

  private:
    struct KnownBar {
        Address mac;
        uint32_t lastSeenMs;
        bool valid;
    };

    static void onHostPacket(const uint8_t *packet, size_t len, const RxInfo &info, void *ctx);
    static void onRadioPacket(const uint8_t *packet, size_t len, const RxInfo &info, void *ctx);
    void handleHost(const uint8_t *packet, size_t len);
    void handleRadio(const uint8_t *packet, size_t len, const RxInfo &info);
    void configById(uint8_t barId, const uint8_t *packet, size_t len);
    void broadcastToBars(const uint8_t *packet, size_t len);
    void unicastWithRetries(const Address &to, const uint8_t *packet, size_t len);
    void sendToHost(stinger_msg_t &msg);

    Transport &host_;
    Transport &radio_;
    GatewayConfig config_;
    KnownBar bars_[STINGER_MAX_BAR_ID + 1] = {};
    uint32_t nowMs_ = 0;
    uint32_t lastStatsMs_ = 0;
    bool statsSent_ = false;
    uint16_t seq_ = 0;
    uint32_t txFailures_ = 0;
};

} // namespace stinger
