// Gateway forwarding rules (PROTOCOL.md section 5) with fake host and radio links.
#include <unity.h>

#include "fake_transport.h"
#include "gateway_logic.h"

using namespace stinger;

namespace {

const uint8_t kMacA[6] = {0x02, 0x53, 0x54, 0x00, 0x00, 0xA1};
const uint8_t kMacB[6] = {0x02, 0x53, 0x54, 0x00, 0x00, 0xB2};
const GatewayConfig kConfig = {3, 1000, 3000};

struct Harness {
    FakeTransport host;
    FakeTransport radio;
    GatewayLogic gateway{host, radio, kConfig};

    Harness() {
        gateway.begin();
        gateway.loop(0); // first STATS goes out at boot
        host.sent.clear();
    }

    void barSaysHello(uint8_t id, const uint8_t *mac, uint32_t nowMs, int8_t rssi = -60) {
        gateway.loop(nowMs);
        stinger_msg_t msg = {};
        msg.type = STINGER_MSG_HELLO;
        msg.u.hello = {id, mac, 60, 0x0100, 0};
        radio.inject(encodePacket(msg), Address::of(mac), rssi);
    }

    stinger_msg_t lastToHost() {
        TEST_ASSERT_FALSE(host.sent.empty());
        static std::vector<uint8_t> keep; // decoded fields point into the packet
        keep = host.sent.back().packet;
        stinger_msg_t msg;
        TEST_ASSERT_EQUAL_INT(STINGER_OK, stinger_decode(keep.data(), keep.size(), &msg));
        return msg;
    }
};

stinger_msg_t config(uint8_t barId, uint8_t key, uint16_t value) {
    stinger_msg_t msg = {};
    msg.type = STINGER_MSG_CONFIG;
    msg.u.config = {barId, key, value};
    return msg;
}

} // namespace

void setUp() {}
void tearDown() {}

void test_frame_and_show_are_broadcast_unchanged() {
    Harness h;
    const uint8_t rgb[3] = {1, 2, 3};
    stinger_msg_t frame = {};
    frame.type = STINGER_MSG_FRAME;
    frame.u.frame = {1, 1, rgb};
    stinger_msg_t show = {};
    show.type = STINGER_MSG_SHOW;
    show.u.show.frame_seq = 9;
    h.host.inject(encodePacket(frame));
    h.host.inject(encodePacket(show));
    TEST_ASSERT_EQUAL_size_t(2, h.radio.sent.size());
    TEST_ASSERT_TRUE(h.radio.sent[0].to.isBroadcast());
    TEST_ASSERT_TRUE(h.radio.sent[0].packet == encodePacket(frame));
    TEST_ASSERT_TRUE(h.radio.sent[1].packet == encodePacket(show));
}

void test_ping_is_answered_with_pong() {
    Harness h;
    stinger_msg_t ping = {};
    ping.type = STINGER_MSG_PING;
    ping.u.ping.t_ms = 123456;
    h.host.inject(encodePacket(ping));
    stinger_msg_t reply = h.lastToHost();
    TEST_ASSERT_EQUAL_UINT8(STINGER_MSG_PONG, reply.type);
    TEST_ASSERT_EQUAL_UINT32(123456, reply.u.ping.t_ms);
    TEST_ASSERT_TRUE(h.radio.sent.empty());
}

void test_hello_is_forwarded_with_rssi() {
    Harness h;
    h.barSaysHello(4, kMacA, 10, -55);
    stinger_msg_t hello = h.lastToHost();
    TEST_ASSERT_EQUAL_UINT8(STINGER_MSG_HELLO, hello.type);
    TEST_ASSERT_EQUAL_UINT8(4, hello.u.hello.bar_id);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(kMacA, hello.u.hello.mac, 6);
    TEST_ASSERT_EQUAL_INT8(-55, hello.u.hello.rssi);
}

void test_config_by_id_goes_to_the_mac_from_hello() {
    Harness h;
    h.barSaysHello(4, kMacA, 10);
    h.host.inject(encodePacket(config(4, STINGER_KEY_BRIGHTNESS_CAP, 100)));
    TEST_ASSERT_EQUAL_size_t(1, h.radio.sent.size());
    TEST_ASSERT_TRUE(h.radio.sent[0].to == Address::of(kMacA));
}

void test_config_to_unknown_id_counts_a_failure() {
    Harness h;
    h.host.inject(encodePacket(config(9, STINGER_KEY_BRIGHTNESS_CAP, 100)));
    TEST_ASSERT_TRUE(h.radio.sent.empty());
    TEST_ASSERT_EQUAL_UINT32(1, h.gateway.txFailures());
}

void test_config_all_reaches_only_recently_seen_bars() {
    Harness h;
    h.barSaysHello(1, kMacA, 0);
    h.barSaysHello(2, kMacB, 2500);
    h.gateway.loop(3500);
    h.host.inject(encodePacket(config(STINGER_BAR_ALL, STINGER_KEY_FAILSAFE_TIMEOUT_MS, 1000)));
    TEST_ASSERT_EQUAL_size_t(1, h.radio.sent.size());
    TEST_ASSERT_TRUE(h.radio.sent[0].to == Address::of(kMacB));
}

void test_unacknowledged_config_is_retried_three_times() {
    Harness h;
    h.radio.acks = false;
    stinger_msg_t msg = {};
    msg.type = STINGER_MSG_CONFIG_MAC;
    msg.u.config_mac = {kMacA, STINGER_KEY_ASSIGN_ID, 3};
    h.host.inject(encodePacket(msg));
    TEST_ASSERT_EQUAL_size_t(4, h.radio.sent.size());
    TEST_ASSERT_EQUAL_UINT32(1, h.gateway.txFailures());
}

void test_config_mac_to_everyone_is_broadcast() {
    Harness h;
    const uint8_t all[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    stinger_msg_t msg = {};
    msg.type = STINGER_MSG_CONFIG_MAC;
    msg.u.config_mac = {all, STINGER_KEY_BRIGHTNESS_CAP, 50};
    h.host.inject(encodePacket(msg));
    TEST_ASSERT_EQUAL_size_t(1, h.radio.sent.size());
    TEST_ASSERT_TRUE(h.radio.sent[0].to.isBroadcast());
}

void test_stats_every_interval_with_link_counters() {
    Harness h;
    h.host.fakeCounters = {12, 3};
    h.gateway.loop(999);
    TEST_ASSERT_TRUE(h.host.sent.empty());
    h.gateway.loop(1000);
    stinger_msg_t stats = h.lastToHost();
    TEST_ASSERT_EQUAL_UINT8(STINGER_MSG_STATS, stats.type);
    TEST_ASSERT_EQUAL_UINT32(12, stats.u.stats.rx_packets);
    TEST_ASSERT_EQUAL_UINT32(3, stats.u.stats.crc_errors);
}

void test_invalid_packets_from_either_side_are_dropped() {
    Harness h;
    h.host.inject({0x7E, 0x01, 0x00});
    h.radio.inject({0x01, 0x01, 0x00, 0x00, 0x00}); // FRAME for bar 0
    TEST_ASSERT_TRUE(h.radio.sent.empty());
    TEST_ASSERT_TRUE(h.host.sent.empty());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_frame_and_show_are_broadcast_unchanged);
    RUN_TEST(test_ping_is_answered_with_pong);
    RUN_TEST(test_hello_is_forwarded_with_rssi);
    RUN_TEST(test_config_by_id_goes_to_the_mac_from_hello);
    RUN_TEST(test_config_to_unknown_id_counts_a_failure);
    RUN_TEST(test_config_all_reaches_only_recently_seen_bars);
    RUN_TEST(test_unacknowledged_config_is_retried_three_times);
    RUN_TEST(test_config_mac_to_everyone_is_broadcast);
    RUN_TEST(test_stats_every_interval_with_link_counters);
    RUN_TEST(test_invalid_packets_from_either_side_are_dropped);
    return UNITY_END();
}
