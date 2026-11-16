#include "bar_logic.h"

namespace stinger {

BarLogic::BarLogic(Transport &link, IdStore &ids, const uint8_t *mac, const BarConfig &config)
    : link_(link), ids_(ids), config_(config), runtimeCap_(config.hardLimit),
      failsafeTimeoutMs_(config.failsafeTimeoutMs) {
    memcpy(mac_, mac, sizeof(mac_));
    if (config_.ledCount > STINGER_MAX_FRAME_LEDS) {
        config_.ledCount = STINGER_MAX_FRAME_LEDS;
    }
}

void BarLogic::begin() {
    uint8_t stored = ids_.load();
    barId_ = stored <= STINGER_MAX_BAR_ID ? stored : STINGER_BAR_UNASSIGNED;
    link_.onReceive(onPacket, this);
}

void BarLogic::loop(uint32_t nowMs) {
    nowMs_ = nowMs;
    link_.poll();
    stinger_msg_t hello;
    if (pollHello(nowMs, &hello)) {
        uint8_t packet[STINGER_MAX_PACKET];
        int n = stinger_encode(&hello, packet, sizeof(packet));
        if (n > 0) {
            link_.send(Address::broadcast(), packet, static_cast<size_t>(n));
        }
    }
}

void BarLogic::onPacket(const uint8_t *packet, size_t len, const RxInfo &, void *ctx) {
    BarLogic *self = static_cast<BarLogic *>(ctx);
    self->receive(packet, len, self->nowMs_);
}

void BarLogic::receive(const uint8_t *packet, size_t len, uint32_t nowMs) {
    stinger_msg_t msg;
    if (stinger_decode(packet, len, &msg) != STINGER_OK) {
        return;
    }
    switch (msg.type) {
    case STINGER_MSG_FRAME:
        onFrame(msg.u.frame, nowMs);
        break;
    case STINGER_MSG_SHOW:
        memcpy(shown_, pending_, 3u * config_.ledCount);
        break;
    case STINGER_MSG_CONFIG:
        if (msg.u.config.bar_id == barId_ || msg.u.config.bar_id == STINGER_BAR_ALL) {
            applyConfig(msg.u.config.key, msg.u.config.value);
        }
        break;
    case STINGER_MSG_CONFIG_MAC: {
        Address to = Address::of(msg.u.config_mac.mac);
        if (to.isBroadcast() || to == Address::of(mac_)) {
            applyConfig(msg.u.config_mac.key, msg.u.config_mac.value);
        }
        break;
    }
    default:
        break;
    }
}

void BarLogic::onFrame(const stinger_frame_t &frame, uint32_t nowMs) {
    if (barId_ == STINGER_BAR_UNASSIGNED || frame.bar_id != barId_) {
        return;
    }
    size_t want = 3u * config_.ledCount;
    size_t got = 3u * frame.led_count;
    size_t n = got < want ? got : want;
    memcpy(pending_, frame.rgb, n);
    memset(pending_ + n, 0, want - n);
    everHadFrame_ = true;
    lastFrameMs_ = nowMs;
}

void BarLogic::applyConfig(uint8_t key, uint16_t value) {
    switch (key) {
    case STINGER_KEY_BRIGHTNESS_CAP:
        runtimeCap_ = value > kFullScale ? kFullScale : static_cast<uint8_t>(value);
        break;
    case STINGER_KEY_FAILSAFE_TIMEOUT_MS:
        failsafeTimeoutMs_ = value < kMinFailsafeTimeoutMs ? kMinFailsafeTimeoutMs : value;
        break;
    case STINGER_KEY_ASSIGN_ID:
        if (value <= STINGER_MAX_BAR_ID) {
            barId_ = static_cast<uint8_t>(value);
            ids_.save(barId_);
            helloSent_ = false; // announce the new ID at once
        }
        break;
    default:
        break;
    }
}

bool BarLogic::pollHello(uint32_t nowMs, stinger_msg_t *hello) {
    if (helloSent_ && nowMs - lastHelloMs_ < config_.helloIntervalMs) {
        return false;
    }
    helloSent_ = true;
    lastHelloMs_ = nowMs;
    *hello = stinger_msg_t{};
    hello->type = STINGER_MSG_HELLO;
    hello->seq = ++seq_;
    hello->u.hello = {barId_, mac_, config_.ledCount, config_.fwVersion, 0};
    return true;
}

void BarLogic::render(uint32_t nowMs, uint8_t *rgb) const {
    size_t n = 3u * config_.ledCount;
    if (barId_ == STINGER_BAR_UNASSIGNED) {
        memset(rgb, breathLevel(nowMs) * cap() / kFullScale, n);
        return;
    }
    uint32_t scale = static_cast<uint32_t>(cap()) * fadeLevel(nowMs);
    for (size_t i = 0; i < n; i++) {
        rgb[i] = static_cast<uint8_t>(shown_[i] * scale / (kFullScale * kFullScale));
    }
}

uint8_t BarLogic::fadeLevel(uint32_t nowMs) const {
    if (!everHadFrame_) {
        return 0;
    }
    uint32_t silent = nowMs - lastFrameMs_;
    if (silent <= failsafeTimeoutMs_) {
        return kFullScale;
    }
    uint32_t late = silent - failsafeTimeoutMs_;
    if (late >= config_.failsafeFadeMs) {
        return 0;
    }
    return static_cast<uint8_t>(kFullScale - kFullScale * late / config_.failsafeFadeMs);
}

uint8_t BarLogic::breathLevel(uint32_t nowMs) {
    const uint32_t half = kBreathPeriodMs / 2;
    uint32_t phase = nowMs % kBreathPeriodMs;
    uint32_t ramp = phase < half ? phase : kBreathPeriodMs - phase;
    return static_cast<uint8_t>(kBreathPeak * ramp / half);
}

} // namespace stinger
