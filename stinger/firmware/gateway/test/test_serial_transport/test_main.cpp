// SerialTransport: COBS + CRC framing over a byte stream, as the gateway's host link uses it.
#include <unity.h>

#include <vector>

#include "golden_vectors.h"
#include "serial_transport.h"

using namespace stinger;

namespace {

class BufferStream : public ByteStream {
  public:
    std::vector<uint8_t> input;
    std::vector<uint8_t> output;
    size_t readPos = 0;
    size_t maxChunk = 7; // deliver input in small, odd-sized pieces

    size_t read(uint8_t *buf, size_t max) override {
        size_t n = input.size() - readPos;
        n = n < max ? n : max;
        n = n < maxChunk ? n : maxChunk;
        for (size_t i = 0; i < n; i++) {
            buf[i] = input[readPos++];
        }
        return n;
    }
    size_t write(const uint8_t *data, size_t len) override {
        output.insert(output.end(), data, data + len);
        return len;
    }
};

std::vector<std::vector<uint8_t>> received;

void collect(const uint8_t *packet, size_t len, const RxInfo &, void *) {
    received.emplace_back(packet, packet + len);
}

} // namespace

void setUp() {
    received.clear();
}
void tearDown() {}

void test_send_writes_golden_wire_bytes() {
    BufferStream stream;
    SerialTransport link(stream);
    for (size_t i = 0; i < GOLDEN_COUNT(golden_packets); i++) {
        const golden_packet_t &v = golden_packets[i];
        stream.output.clear();
        TEST_ASSERT_TRUE(link.send(Address::broadcast(), v.packet, v.packet_len));
        TEST_ASSERT_EQUAL_size_t(v.wire_len, stream.output.size());
        TEST_ASSERT_EQUAL_UINT8_ARRAY(v.wire, stream.output.data(), v.wire_len);
    }
}

void test_poll_delivers_packets_and_counts_errors() {
    BufferStream stream;
    for (size_t i = 0; i < GOLDEN_COUNT(golden_packets); i++) {
        stream.input.insert(stream.input.end(), golden_packets[i].wire,
                            golden_packets[i].wire + golden_packets[i].wire_len);
    }
    const golden_invalid_t &bad = golden_invalid_wire[0];
    stream.input.insert(stream.input.end(), bad.bytes, bad.bytes + bad.len);

    SerialTransport link(stream);
    link.onReceive(collect, nullptr);
    link.poll();

    TEST_ASSERT_EQUAL_size_t(GOLDEN_COUNT(golden_packets), received.size());
    for (size_t i = 0; i < received.size(); i++) {
        TEST_ASSERT_EQUAL_size_t(golden_packets[i].packet_len, received[i].size());
        TEST_ASSERT_EQUAL_UINT8_ARRAY(golden_packets[i].packet, received[i].data(),
                                      received[i].size());
    }
    TEST_ASSERT_EQUAL_UINT32(GOLDEN_COUNT(golden_packets), link.counters().rxPackets);
    TEST_ASSERT_EQUAL_UINT32(1, link.counters().rxErrors);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_send_writes_golden_wire_bytes);
    RUN_TEST(test_poll_delivers_packets_and_counts_errors);
    return UNITY_END();
}
