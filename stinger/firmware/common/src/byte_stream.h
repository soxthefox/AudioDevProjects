// A bidirectional byte stream (a UART, a USB CDC port, a test buffer).
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace stinger {

class ByteStream {
  public:
    virtual ~ByteStream() = default;
    // Non-blocking: returns how many bytes were copied into buf (0 if none are waiting).
    virtual size_t read(uint8_t *buf, size_t max) = 0;
    // Returns how many bytes were accepted.
    virtual size_t write(const uint8_t *data, size_t len) = 0;
};

} // namespace stinger
