// Stinger packets over a byte stream, framed with COBS + CRC-16 (PROTOCOL.md section 2).
#pragma once

#include "byte_stream.h"
#include "transport.h"

namespace stinger {

class SerialTransport : public Transport {
  public:
    explicit SerialTransport(ByteStream &stream);

    bool send(const Address &to, const uint8_t *packet, size_t len) override;
    void onReceive(ReceiveHandler handler, void *ctx) override;
    void poll() override;
    LinkCounters counters() const override { return counters_; }

  private:
    ByteStream &stream_;
    stinger_wire_decoder_t decoder_;
    ReceiveHandler handler_ = nullptr;
    void *ctx_ = nullptr;
    LinkCounters counters_ = {0, 0};
};

} // namespace stinger
