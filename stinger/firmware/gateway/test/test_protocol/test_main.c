/* The C codec against the golden vectors (PROTOCOL.md section 9). */
#include <string.h>
#include <unity.h>

#include "golden_vectors.h"
#include "stinger_proto.h"

void setUp(void) {}
void tearDown(void) {}

static void assert_msg_equal(const stinger_msg_t *want, const stinger_msg_t *got) {
    TEST_ASSERT_EQUAL_UINT8(want->type, got->type);
    TEST_ASSERT_EQUAL_UINT16(want->seq, got->seq);
    switch (want->type) {
    case STINGER_MSG_FRAME:
        TEST_ASSERT_EQUAL_UINT8(want->u.frame.bar_id, got->u.frame.bar_id);
        TEST_ASSERT_EQUAL_UINT8(want->u.frame.led_count, got->u.frame.led_count);
        if (want->u.frame.led_count > 0) {
            TEST_ASSERT_EQUAL_UINT8_ARRAY(want->u.frame.rgb, got->u.frame.rgb,
                                          3u * want->u.frame.led_count);
        }
        break;
    case STINGER_MSG_SHOW:
        TEST_ASSERT_EQUAL_UINT16(want->u.show.frame_seq, got->u.show.frame_seq);
        break;
    case STINGER_MSG_CONFIG:
        TEST_ASSERT_EQUAL_UINT8(want->u.config.bar_id, got->u.config.bar_id);
        TEST_ASSERT_EQUAL_UINT8(want->u.config.key, got->u.config.key);
        TEST_ASSERT_EQUAL_UINT16(want->u.config.value, got->u.config.value);
        break;
    case STINGER_MSG_CONFIG_MAC:
        TEST_ASSERT_EQUAL_UINT8_ARRAY(want->u.config_mac.mac, got->u.config_mac.mac,
                                      STINGER_MAC_SIZE);
        TEST_ASSERT_EQUAL_UINT8(want->u.config_mac.key, got->u.config_mac.key);
        TEST_ASSERT_EQUAL_UINT16(want->u.config_mac.value, got->u.config_mac.value);
        break;
    case STINGER_MSG_PING:
    case STINGER_MSG_PONG:
        TEST_ASSERT_EQUAL_UINT32(want->u.ping.t_ms, got->u.ping.t_ms);
        break;
    case STINGER_MSG_HELLO:
        TEST_ASSERT_EQUAL_UINT8(want->u.hello.bar_id, got->u.hello.bar_id);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(want->u.hello.mac, got->u.hello.mac, STINGER_MAC_SIZE);
        TEST_ASSERT_EQUAL_UINT8(want->u.hello.led_count, got->u.hello.led_count);
        TEST_ASSERT_EQUAL_UINT16(want->u.hello.fw_version, got->u.hello.fw_version);
        TEST_ASSERT_EQUAL_INT8(want->u.hello.rssi, got->u.hello.rssi);
        break;
    case STINGER_MSG_STATS:
        TEST_ASSERT_EQUAL_UINT32(want->u.stats.rx_packets, got->u.stats.rx_packets);
        TEST_ASSERT_EQUAL_UINT32(want->u.stats.crc_errors, got->u.stats.crc_errors);
        TEST_ASSERT_EQUAL_UINT32(want->u.stats.tx_failures, got->u.stats.tx_failures);
        break;
    default:
        TEST_FAIL_MESSAGE("unexpected message type in vector");
    }
}

/* Feed a whole byte string; return the last status and fill the outputs. */
static stinger_wire_status_t feed_all(const uint8_t *bytes, size_t len, const uint8_t **packet,
                                      size_t *packet_len, stinger_err_t *err) {
    stinger_wire_decoder_t dec;
    stinger_wire_decoder_init(&dec);
    stinger_wire_status_t last = STINGER_WIRE_NONE;
    for (size_t i = 0; i < len; i++) {
        stinger_wire_status_t s = stinger_wire_feed(&dec, bytes[i], packet, packet_len, err);
        if (s != STINGER_WIRE_NONE) {
            last = s;
            if (s == STINGER_WIRE_PACKET) {
                /* Copy out: the decoder's buffer is reused on the next feed. */
                static uint8_t copy[STINGER_MAX_PACKET];
                memcpy(copy, *packet, *packet_len);
                *packet = copy;
            }
        }
    }
    return last;
}

static void test_crc_check_value(void) {
    const char *input = GOLDEN_CRC_INPUT;
    TEST_ASSERT_EQUAL_HEX16(GOLDEN_CRC_VALUE, stinger_crc16((const uint8_t *)input, strlen(input)));
}

static void test_cobs_vectors(void) {
    uint8_t out[600];
    for (size_t i = 0; i < GOLDEN_COUNT(golden_cobs); i++) {
        const golden_cobs_t *c = &golden_cobs[i];
        int n = stinger_cobs_encode(c->decoded, c->decoded_len, out, sizeof(out));
        TEST_ASSERT_EQUAL_INT(c->encoded_len, n);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(c->encoded, out, c->encoded_len);
        n = stinger_cobs_decode(c->encoded, c->encoded_len, out, sizeof(out));
        TEST_ASSERT_EQUAL_INT(c->decoded_len, n);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(c->decoded, out, c->decoded_len);
    }
}

static void test_packets_decode_to_meaning(void) {
    for (size_t i = 0; i < GOLDEN_COUNT(golden_packets); i++) {
        const golden_packet_t *v = &golden_packets[i];
        stinger_msg_t msg;
        TEST_ASSERT_EQUAL_INT_MESSAGE(STINGER_OK, stinger_decode(v->packet, v->packet_len, &msg),
                                      v->name);
        assert_msg_equal(&v->msg, &msg);
    }
}

static void test_meaning_encodes_to_packet(void) {
    uint8_t out[STINGER_MAX_PACKET];
    for (size_t i = 0; i < GOLDEN_COUNT(golden_packets); i++) {
        const golden_packet_t *v = &golden_packets[i];
        int n = stinger_encode(&v->msg, out, sizeof(out));
        TEST_ASSERT_EQUAL_INT_MESSAGE(v->packet_len, n, v->name);
        TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(v->packet, out, v->packet_len, v->name);
    }
}

static void test_wire_framing(void) {
    uint8_t out[STINGER_MAX_WIRE_FRAME];
    for (size_t i = 0; i < GOLDEN_COUNT(golden_packets); i++) {
        const golden_packet_t *v = &golden_packets[i];
        int n = stinger_wire_encode(v->packet, v->packet_len, out, sizeof(out));
        TEST_ASSERT_EQUAL_INT_MESSAGE(v->wire_len, n, v->name);
        TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(v->wire, out, v->wire_len, v->name);

        const uint8_t *packet = NULL;
        size_t len = 0;
        stinger_err_t err = STINGER_OK;
        TEST_ASSERT_EQUAL_INT(STINGER_WIRE_PACKET,
                              feed_all(v->wire, v->wire_len, &packet, &len, &err));
        TEST_ASSERT_EQUAL_size_t(v->packet_len, len);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(v->packet, packet, len);
    }
}

static void test_invalid_wire(void) {
    for (size_t i = 0; i < GOLDEN_COUNT(golden_invalid_wire); i++) {
        const golden_invalid_t *v = &golden_invalid_wire[i];
        const uint8_t *packet = NULL;
        size_t len = 0;
        stinger_err_t err = STINGER_OK;
        TEST_ASSERT_EQUAL_INT_MESSAGE(STINGER_WIRE_ERROR,
                                      feed_all(v->bytes, v->len, &packet, &len, &err), v->name);
        TEST_ASSERT_EQUAL_INT_MESSAGE(v->error, err, v->name);
    }
}

static void test_invalid_packets(void) {
    for (size_t i = 0; i < GOLDEN_COUNT(golden_invalid_packets); i++) {
        const golden_invalid_t *v = &golden_invalid_packets[i];
        stinger_msg_t msg;
        TEST_ASSERT_EQUAL_INT_MESSAGE(v->error, stinger_decode(v->bytes, v->len, &msg), v->name);
    }
}

static void test_round_trip_every_led_count(void) {
    uint8_t rgb[3 * STINGER_MAX_FRAME_LEDS];
    for (size_t i = 0; i < sizeof(rgb); i++) {
        rgb[i] = (uint8_t)(i * 7); /* includes zeros, so COBS has work to do */
    }
    for (unsigned leds = 0; leds <= STINGER_MAX_FRAME_LEDS; leds++) {
        stinger_msg_t msg = {.type = STINGER_MSG_FRAME, .seq = (uint16_t)(leds * 997)};
        msg.u.frame.bar_id = (uint8_t)(1 + leds % STINGER_MAX_BAR_ID);
        msg.u.frame.led_count = (uint8_t)leds;
        msg.u.frame.rgb = rgb;
        uint8_t packet[STINGER_MAX_PACKET];
        uint8_t wire[STINGER_MAX_WIRE_FRAME];
        int plen = stinger_encode(&msg, packet, sizeof(packet));
        TEST_ASSERT_GREATER_THAN_INT(0, plen);
        int wlen = stinger_wire_encode(packet, (size_t)plen, wire, sizeof(wire));
        TEST_ASSERT_GREATER_THAN_INT(0, wlen);

        const uint8_t *got = NULL;
        size_t len = 0;
        stinger_err_t err = STINGER_OK;
        TEST_ASSERT_EQUAL_INT(STINGER_WIRE_PACKET, feed_all(wire, (size_t)wlen, &got, &len, &err));
        stinger_msg_t back;
        TEST_ASSERT_EQUAL_INT(STINGER_OK, stinger_decode(got, len, &back));
        assert_msg_equal(&msg, &back);
    }
}

static void test_every_single_bit_flip_is_rejected(void) {
    for (size_t i = 0; i < GOLDEN_COUNT(golden_packets); i++) {
        const golden_packet_t *v = &golden_packets[i];
        for (size_t pos = 0; pos + 1 < v->wire_len; pos++) {
            for (int bit = 0; bit < 8; bit++) {
                uint8_t corrupt[STINGER_MAX_WIRE_FRAME];
                memcpy(corrupt, v->wire, v->wire_len);
                corrupt[pos] ^= (uint8_t)(1u << bit);
                stinger_wire_decoder_t dec;
                stinger_wire_decoder_init(&dec);
                for (size_t k = 0; k < v->wire_len; k++) {
                    const uint8_t *packet = NULL;
                    size_t len = 0;
                    stinger_err_t err = STINGER_OK;
                    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(
                        STINGER_WIRE_PACKET,
                        stinger_wire_feed(&dec, corrupt[k], &packet, &len, &err), v->name);
                }
            }
        }
    }
}

static void test_encode_refuses_small_buffers(void) {
    const golden_packet_t *v = &golden_packets[0];
    uint8_t out[STINGER_MAX_WIRE_FRAME];
    TEST_ASSERT_EQUAL_INT(STINGER_ERR_NO_SPACE, stinger_encode(&v->msg, out, v->packet_len - 1));
    TEST_ASSERT_EQUAL_INT(STINGER_ERR_NO_SPACE,
                          stinger_wire_encode(v->packet, v->packet_len, out, v->wire_len - 1));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_crc_check_value);
    RUN_TEST(test_cobs_vectors);
    RUN_TEST(test_packets_decode_to_meaning);
    RUN_TEST(test_meaning_encodes_to_packet);
    RUN_TEST(test_wire_framing);
    RUN_TEST(test_invalid_wire);
    RUN_TEST(test_invalid_packets);
    RUN_TEST(test_round_trip_every_led_count);
    RUN_TEST(test_every_single_bit_flip_is_rejected);
    RUN_TEST(test_encode_refuses_small_buffers);
    return UNITY_END();
}
