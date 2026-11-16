/*
 * Stinger protocol codec (C). Normative spec: PROTOCOL.md.
 *
 * No dynamic allocation. Decoded messages point into the caller's input
 * buffer (FRAME rgb, MAC addresses), so the input must outlive the message.
 */
#ifndef STINGER_PROTO_H
#define STINGER_PROTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sizes and ranges (PROTOCOL.md sections 3, 4.1 and 8). */
#define STINGER_HEADER_SIZE 3
#define STINGER_MAX_PACKET 250
#define STINGER_CRC_SIZE 2
#define STINGER_MAX_FRAME_LEDS 81
#define STINGER_MAX_BAR_ID 16
#define STINGER_BAR_UNASSIGNED 0
#define STINGER_BAR_ALL 0xFF
#define STINGER_MAC_SIZE 6
/* COBS adds one byte per 254 input bytes, at least one. */
#define STINGER_COBS_MAX_ENCODED(n) ((n) + ((n) / 254) + 1)
#define STINGER_MAX_WIRE_BODY STINGER_COBS_MAX_ENCODED(STINGER_MAX_PACKET)
/* Encoded body plus the 0x00 delimiter. */
#define STINGER_MAX_WIRE_FRAME (STINGER_MAX_WIRE_BODY + 1)

typedef enum {
    STINGER_MSG_FRAME = 0x01,
    STINGER_MSG_SHOW = 0x02,
    STINGER_MSG_CONFIG = 0x03,
    STINGER_MSG_PING = 0x04,
    STINGER_MSG_CONFIG_MAC = 0x05,
    STINGER_MSG_HELLO = 0x81,
    STINGER_MSG_PONG = 0x82,
    STINGER_MSG_STATS = 0x83,
} stinger_msg_type_t;

typedef enum {
    STINGER_KEY_BRIGHTNESS_CAP = 0x01,
    STINGER_KEY_FAILSAFE_TIMEOUT_MS = 0x02,
    STINGER_KEY_ASSIGN_ID = 0x03,
} stinger_config_key_t;

typedef enum {
    STINGER_OK = 0,
    /* Packet layer, in the order PROTOCOL.md section 3 checks them. */
    STINGER_ERR_TOO_SHORT = -1,
    STINGER_ERR_UNKNOWN_TYPE = -2,
    STINGER_ERR_BAD_LENGTH = -3,
    STINGER_ERR_BAD_FIELD = -4,
    /* Serial framing layer. */
    STINGER_ERR_FRAMING = -5,
    STINGER_ERR_CRC = -6,
    /* Caller's output buffer is too small. */
    STINGER_ERR_NO_SPACE = -7,
} stinger_err_t;

typedef struct {
    uint8_t bar_id;
    uint8_t led_count;
    const uint8_t *rgb; /* 3 * led_count bytes, R G B per pixel */
} stinger_frame_t;

typedef struct {
    uint16_t frame_seq;
} stinger_show_t;

typedef struct {
    uint8_t bar_id;
    uint8_t key;
    uint16_t value;
} stinger_config_t;

typedef struct {
    const uint8_t *mac;
    uint8_t key;
    uint16_t value;
} stinger_config_mac_t;

typedef struct {
    uint32_t t_ms;
} stinger_ping_t; /* also PONG */

typedef struct {
    uint8_t bar_id;
    const uint8_t *mac;
    uint8_t led_count;
    uint16_t fw_version;
    int8_t rssi;
} stinger_hello_t;

typedef struct {
    uint32_t rx_packets;
    uint32_t crc_errors;
    uint32_t tx_failures;
} stinger_stats_t;

typedef struct {
    uint8_t type; /* stinger_msg_type_t */
    uint16_t seq;
    union {
        stinger_frame_t frame;
        stinger_show_t show;
        stinger_config_t config;
        stinger_ping_t ping; /* PING and PONG */
        stinger_config_mac_t config_mac;
        stinger_hello_t hello;
        stinger_stats_t stats;
    } u;
} stinger_msg_t;

/* CRC-16/CCITT-FALSE. */
uint16_t stinger_crc16(const uint8_t *data, size_t len);

/* COBS. Return the output length, or a negative stinger_err_t. */
int stinger_cobs_encode(const uint8_t *in, size_t len, uint8_t *out, size_t cap);
int stinger_cobs_decode(const uint8_t *in, size_t len, uint8_t *out, size_t cap);

/* Packet layer. encode returns the packet length or a negative error. */
int stinger_encode(const stinger_msg_t *msg, uint8_t *out, size_t cap);
stinger_err_t stinger_decode(const uint8_t *packet, size_t len, stinger_msg_t *msg);

/* Serial framing: packet -> COBS(packet || crc) || 0x00. */
int stinger_wire_encode(const uint8_t *packet, size_t len, uint8_t *out, size_t cap);

/* Byte-at-a-time serial receiver. */
typedef struct {
    uint8_t buf[STINGER_MAX_WIRE_BODY];
    size_t len;
    bool overflow;
    uint8_t packet[STINGER_MAX_PACKET + STINGER_CRC_SIZE];
} stinger_wire_decoder_t;

typedef enum {
    STINGER_WIRE_NONE = 0,   /* need more bytes */
    STINGER_WIRE_PACKET = 1, /* a packet is ready */
    STINGER_WIRE_ERROR = 2,  /* a frame was dropped; see *err */
} stinger_wire_status_t;

void stinger_wire_decoder_init(stinger_wire_decoder_t *dec);

/*
 * Feed one byte. On STINGER_WIRE_PACKET, *packet and *len describe the
 * packet (CRC removed), valid until the next call. On STINGER_WIRE_ERROR,
 * *err is STINGER_ERR_FRAMING or STINGER_ERR_CRC.
 */
stinger_wire_status_t stinger_wire_feed(stinger_wire_decoder_t *dec, uint8_t byte,
                                        const uint8_t **packet, size_t *len, stinger_err_t *err);

#ifdef __cplusplus
}
#endif

#endif /* STINGER_PROTO_H */
