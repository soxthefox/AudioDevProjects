// The link a node sends and receives Stinger packets on (PROTOCOL.md section 1).
//
// Implementations: SerialTransport (COBS + CRC over a byte stream: USB serial, or a future
// wired bar link) and EspNowTransport (radio). Logic code only sees this interface.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "stinger_proto.h"

namespace stinger {

struct Address {
    uint8_t mac[STINGER_MAC_SIZE];

    static Address broadcast() {
        Address a;
        memset(a.mac, 0xFF, sizeof(a.mac));
        return a;
    }
    static Address of(const uint8_t *mac) {
        Address a;
        memcpy(a.mac, mac, sizeof(a.mac));
        return a;
    }
    bool isBroadcast() const { return *this == broadcast(); }
    bool operator==(const Address &other) const { return memcmp(mac, other.mac, sizeof(mac)) == 0; }
};

struct RxInfo {
    Address from; // all-FF when the link has no addresses
    int8_t rssi;  // 0 = unknown
};

struct LinkCounters {
    uint32_t rxPackets; // valid packets received
    uint32_t rxErrors;  // frames dropped for framing or CRC errors
};

using ReceiveHandler = void (*)(const uint8_t *packet, size_t len, const RxInfo &info, void *ctx);

class Transport {
  public:
    virtual ~Transport() = default;

    // Broadcast: true if the packet was queued. Unicast: true only once the peer acknowledged
    // it. Links without addresses treat every send as a broadcast.
    virtual bool send(const Address &to, const uint8_t *packet, size_t len) = 0;

    // The handler runs inside poll(), in the caller's context, never from an interrupt.
    virtual void onReceive(ReceiveHandler handler, void *ctx) = 0;

    // Delivers every packet received since the last call.
    virtual void poll() = 0;

    virtual LinkCounters counters() const = 0;
};

} // namespace stinger
