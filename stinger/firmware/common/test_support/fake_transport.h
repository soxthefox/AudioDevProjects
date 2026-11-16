// Test double for Transport: records sends, injects received packets. Native tests only.
#pragma once

#include <vector>

#include "transport.h"

namespace stinger {

class FakeTransport : public Transport {
  public:
    struct Sent {
        Address to;
        std::vector<uint8_t> packet;
    };

    std::vector<Sent> sent;
    bool acks = true; // what unicast sends report
    LinkCounters fakeCounters = {0, 0};

    bool send(const Address &to, const uint8_t *packet, size_t len) override {
        sent.push_back(Sent{to, std::vector<uint8_t>(packet, packet + len)});
        return to.isBroadcast() || acks;
    }
    void onReceive(ReceiveHandler handler, void *ctx) override {
        handler_ = handler;
        ctx_ = ctx;
    }
    void poll() override {}
    LinkCounters counters() const override { return fakeCounters; }

    // Delivers a packet as if it had just arrived.
    void inject(const std::vector<uint8_t> &packet, const Address &from = Address::broadcast(),
                int8_t rssi = 0) {
        handler_(packet.data(), packet.size(), RxInfo{from, rssi}, ctx_);
    }

  private:
    ReceiveHandler handler_ = nullptr;
    void *ctx_ = nullptr;
};

// Encodes a message into a packet, failing loudly if the message is invalid.
inline std::vector<uint8_t> encodePacket(const stinger_msg_t &msg) {
    uint8_t buf[STINGER_MAX_PACKET];
    int n = stinger_encode(&msg, buf, sizeof(buf));
    return n > 0 ? std::vector<uint8_t>(buf, buf + n) : std::vector<uint8_t>();
}

} // namespace stinger
