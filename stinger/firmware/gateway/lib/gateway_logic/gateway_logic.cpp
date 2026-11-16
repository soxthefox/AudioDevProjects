#include "gateway_logic.h"

namespace stinger {

GatewayLogic::GatewayLogic(Transport &host, Transport &radio, const GatewayConfig &config)
    : host_(host), radio_(radio), config_(config) {}

void GatewayLogic::begin() {
    host_.onReceive(onHostPacket, this);
    radio_.onReceive(onRadioPacket, this);
}

void GatewayLogic::loop(uint32_t nowMs) {
    nowMs_ = nowMs;
    host_.poll();
    radio_.poll();
    if (statsSent_ && nowMs - lastStatsMs_ < config_.statsIntervalMs) {
        return;
    }
    statsSent_ = true;
    lastStatsMs_ = nowMs;
    LinkCounters host = host_.counters();
    stinger_msg_t msg = {};
    msg.type = STINGER_MSG_STATS;
    msg.u.stats = {host.rxPackets, host.rxErrors, txFailures_};
    sendToHost(msg);
}

void GatewayLogic::onHostPacket(const uint8_t *packet, size_t len, const RxInfo &, void *ctx) {
    static_cast<GatewayLogic *>(ctx)->handleHost(packet, len);
}

void GatewayLogic::onRadioPacket(const uint8_t *packet, size_t len, const RxInfo &info, void *ctx) {
    static_cast<GatewayLogic *>(ctx)->handleRadio(packet, len, info);
}

void GatewayLogic::handleHost(const uint8_t *packet, size_t len) {
    stinger_msg_t msg;
    if (stinger_decode(packet, len, &msg) != STINGER_OK) {
        return;
    }
    switch (msg.type) {
    case STINGER_MSG_FRAME:
    case STINGER_MSG_SHOW:
        broadcastToBars(packet, len);
        break;
    case STINGER_MSG_CONFIG:
        configById(msg.u.config.bar_id, packet, len);
        break;
    case STINGER_MSG_CONFIG_MAC: {
        Address to = Address::of(msg.u.config_mac.mac);
        if (to.isBroadcast()) {
            broadcastToBars(packet, len);
        } else {
            unicastWithRetries(to, packet, len);
        }
        break;
    }
    case STINGER_MSG_PING:
        msg.type = STINGER_MSG_PONG;
        sendToHost(msg);
        break;
    default:
        break; // gateway-to-laptop types are not expected from the laptop
    }
}

void GatewayLogic::handleRadio(const uint8_t *packet, size_t len, const RxInfo &info) {
    stinger_msg_t msg;
    if (stinger_decode(packet, len, &msg) != STINGER_OK || msg.type != STINGER_MSG_HELLO) {
        return;
    }
    uint8_t id = msg.u.hello.bar_id;
    if (id != STINGER_BAR_UNASSIGNED) {
        bars_[id] = KnownBar{Address::of(msg.u.hello.mac), nowMs_, true};
    }
    msg.u.hello.rssi = info.rssi;
    sendToHost(msg);
}

void GatewayLogic::configById(uint8_t barId, const uint8_t *packet, size_t len) {
    if (barId != STINGER_BAR_ALL) {
        if (!bars_[barId].valid) {
            txFailures_++; // never heard from this ID: no MAC to send to
            return;
        }
        unicastWithRetries(bars_[barId].mac, packet, len);
        return;
    }
    for (uint8_t id = 1; id <= STINGER_MAX_BAR_ID; id++) {
        if (bars_[id].valid && nowMs_ - bars_[id].lastSeenMs <= config_.offlineAfterMs) {
            unicastWithRetries(bars_[id].mac, packet, len);
        }
    }
}

void GatewayLogic::broadcastToBars(const uint8_t *packet, size_t len) {
    if (!radio_.send(Address::broadcast(), packet, len)) {
        txFailures_++;
    }
}

void GatewayLogic::unicastWithRetries(const Address &to, const uint8_t *packet, size_t len) {
    for (int attempt = 0; attempt <= config_.configRetries; attempt++) {
        if (radio_.send(to, packet, len)) {
            return;
        }
    }
    txFailures_++;
}

void GatewayLogic::sendToHost(stinger_msg_t &msg) {
    msg.seq = ++seq_;
    uint8_t packet[STINGER_MAX_PACKET];
    int n = stinger_encode(&msg, packet, sizeof(packet));
    if (n > 0) {
        host_.send(Address::broadcast(), packet, static_cast<size_t>(n));
    }
}

} // namespace stinger
