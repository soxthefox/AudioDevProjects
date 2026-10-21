// ByteStream over an Arduino Stream (HardwareSerial, USB CDC).
#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "byte_stream.h"

namespace stinger {

class ArduinoStream : public ByteStream {
  public:
    explicit ArduinoStream(Stream &stream) : stream_(stream) {}

    size_t read(uint8_t *buf, size_t max) override {
        size_t available = static_cast<size_t>(stream_.available());
        return available == 0 ? 0 : stream_.readBytes(buf, available < max ? available : max);
    }
    size_t write(const uint8_t *data, size_t len) override { return stream_.write(data, len); }

  private:
    Stream &stream_;
};

} // namespace stinger

#endif // ARDUINO
