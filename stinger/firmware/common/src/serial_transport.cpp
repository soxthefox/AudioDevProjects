#include "serial_transport.h"

namespace stinger {

namespace {
constexpr size_t kReadChunk = 64;
} // namespace

SerialTransport::SerialTransport(ByteStream &stream) : stream_(stream) {
    stinger_wire_decoder_init(&decoder_);
}

bool SerialTransport::send(const Address &, const uint8_t *packet, size_t len) {
    uint8_t wire[STINGER_MAX_WIRE_FRAME];
    int n = stinger_wire_encode(packet, len, wire, sizeof(wire));
    return n > 0 && stream_.write(wire, static_cast<size_t>(n)) == static_cast<size_t>(n);
}

void SerialTransport::onReceive(ReceiveHandler handler, void *ctx) {
    handler_ = handler;
    ctx_ = ctx;
}

void SerialTransport::poll() {
    uint8_t chunk[kReadChunk];
    size_t n;
    while ((n = stream_.read(chunk, sizeof(chunk))) > 0) {
        for (size_t i = 0; i < n; i++) {
            const uint8_t *packet = nullptr;
            size_t len = 0;
            stinger_err_t err = STINGER_OK;
            switch (stinger_wire_feed(&decoder_, chunk[i], &packet, &len, &err)) {
            case STINGER_WIRE_PACKET:
                counters_.rxPackets++;
                if (handler_) {
                    handler_(packet, len, RxInfo{Address::broadcast(), 0}, ctx_);
                }
                break;
            case STINGER_WIRE_ERROR:
                counters_.rxErrors++;
                break;
            case STINGER_WIRE_NONE:
                break;
            }
        }
    }
}

} // namespace stinger
