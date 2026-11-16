// Bar rules (PROTOCOL.md section 6). No Arduino dependencies.
//
// Mirrors sim/stinger_sim/bar.py exactly, including the integer maths, so the simulator and
// the firmware produce identical pixels. tests/bar_scenarios.json is run against both.
#pragma once

#include "transport.h"

namespace stinger {

struct BarConfig {
    uint8_t ledCount;           // pixels on this bar, at most STINGER_MAX_FRAME_LEDS
    uint8_t hardLimit;          // brightness ceiling, 0-255; the laptop cannot exceed it
    uint32_t failsafeTimeoutMs; // default; CONFIG can change it at runtime
    uint32_t failsafeFadeMs;
    uint32_t helloIntervalMs;
    uint16_t fwVersion;
};

// Where the bar's ID survives power cycles (NVS on the ESP32).
class IdStore {
  public:
    virtual ~IdStore() = default;
    virtual uint8_t load() = 0; // STINGER_BAR_UNASSIGNED if never set
    virtual void save(uint8_t id) = 0;
};

class BarLogic {
  public:
    static constexpr uint8_t kFullScale = 255;
    static constexpr uint32_t kMinFailsafeTimeoutMs = 100;
    // Unassigned bars breathe dim white: a triangle wave from 0 to this level and back.
    static constexpr uint8_t kBreathPeak = 24;
    static constexpr uint32_t kBreathPeriodMs = 4000;

    BarLogic(Transport &link, IdStore &ids, const uint8_t *mac, const BarConfig &config);

    // Loads the stored ID and starts listening.
    void begin();
    // Call from the main loop: receives packets and sends HELLO when due.
    void loop(uint32_t nowMs);

    // The R G B bytes the strip should show now (3 * ledCount), after cap, failsafe and
    // breathing.
    void render(uint32_t nowMs, uint8_t *rgb) const;

    // The pieces loop() is made of, public so tests can drive them step by step.
    void receive(const uint8_t *packet, size_t len, uint32_t nowMs);
    bool pollHello(uint32_t nowMs, stinger_msg_t *hello);

    uint8_t barId() const { return barId_; }
    uint8_t cap() const {
        return runtimeCap_ < config_.hardLimit ? runtimeCap_ : config_.hardLimit;
    }

  private:
    static void onPacket(const uint8_t *packet, size_t len, const RxInfo &info, void *ctx);
    void onFrame(const stinger_frame_t &frame, uint32_t nowMs);
    void applyConfig(uint8_t key, uint16_t value);
    uint8_t fadeLevel(uint32_t nowMs) const;
    static uint8_t breathLevel(uint32_t nowMs);

    Transport &link_;
    IdStore &ids_;
    uint8_t mac_[STINGER_MAC_SIZE];
    BarConfig config_;
    uint8_t barId_ = STINGER_BAR_UNASSIGNED;
    uint8_t runtimeCap_;
    uint32_t failsafeTimeoutMs_;
    uint8_t pending_[3 * STINGER_MAX_FRAME_LEDS] = {};
    uint8_t shown_[3 * STINGER_MAX_FRAME_LEDS] = {};
    bool everHadFrame_ = false;
    uint32_t lastFrameMs_ = 0;
    bool helloSent_ = false;
    uint32_t lastHelloMs_ = 0;
    uint32_t nowMs_ = 0;
    uint16_t seq_ = 0;
};

} // namespace stinger
