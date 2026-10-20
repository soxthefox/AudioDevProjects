/* Stinger protocol codec (C). Normative spec: PROTOCOL.md. */
#include "stinger_proto.h"

#include <string.h>

#define NO_CODE_BYTE ((size_t)-1)

/* Payload sizes of the fixed-length messages. */
enum {
    SHOW_SIZE = 2,
    CONFIG_SIZE = 4,
    PING_SIZE = 4,
    CONFIG_MAC_SIZE = STINGER_MAC_SIZE + 3,
    HELLO_SIZE = STINGER_MAC_SIZE + 5,
    STATS_SIZE = 12,
    FRAME_FIXED_SIZE = 2,
};

static void put_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v) {
    put_u16(p, (uint16_t)(v & 0xFFFF));
    put_u16(p + 2, (uint16_t)(v >> 16));
}

static uint16_t get_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t get_u32(const uint8_t *p) {
    return (uint32_t)get_u16(p) | ((uint32_t)get_u16(p + 2) << 16);
}

uint16_t stinger_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)(data[i] << 8);
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

int stinger_cobs_encode(const uint8_t *in, size_t len, uint8_t *out, size_t cap) {
    if (cap < 1) {
        return STINGER_ERR_NO_SPACE;
    }
    size_t code_at = 0;
    size_t o = 1;
    uint8_t code = 1;
    for (size_t i = 0; i < len; i++) {
        if (in[i] != 0) {
            if (o >= cap) {
                return STINGER_ERR_NO_SPACE;
            }
            out[o++] = in[i];
            code++;
        }
        if (in[i] == 0 || code == 0xFF) {
            out[code_at] = code;
            code = 1;
            /* A full block at the very end is not followed by another code byte. */
            if (in[i] == 0 || i + 1 < len) {
                if (o >= cap) {
                    return STINGER_ERR_NO_SPACE;
                }
                code_at = o++;
            } else {
                code_at = NO_CODE_BYTE;
            }
        }
    }
    if (code_at != NO_CODE_BYTE) {
        out[code_at] = code;
    }
    return (int)o;
}

int stinger_cobs_decode(const uint8_t *in, size_t len, uint8_t *out, size_t cap) {
    size_t i = 0;
    size_t o = 0;
    while (i < len) {
        uint8_t code = in[i++];
        if (code == 0) {
            return STINGER_ERR_FRAMING;
        }
        size_t run = (size_t)code - 1;
        if (i + run > len) {
            return STINGER_ERR_FRAMING;
        }
        if (o + run > cap) {
            return STINGER_ERR_NO_SPACE;
        }
        for (size_t k = 0; k < run; k++) {
            if (in[i + k] == 0) {
                return STINGER_ERR_FRAMING;
            }
            out[o++] = in[i + k];
        }
        i += run;
        if (code != 0xFF && i < len) {
            if (o >= cap) {
                return STINGER_ERR_NO_SPACE;
            }
            out[o++] = 0;
        }
    }
    return (int)o;
}

static bool is_known_key(uint8_t key) {
    return key == STINGER_KEY_BRIGHTNESS_CAP || key == STINGER_KEY_FAILSAFE_TIMEOUT_MS ||
           key == STINGER_KEY_ASSIGN_ID;
}

static bool is_bar_id(uint8_t id) {
    return id >= 1 && id <= STINGER_MAX_BAR_ID;
}

static stinger_err_t check_fields(const stinger_msg_t *msg) {
    switch (msg->type) {
    case STINGER_MSG_FRAME:
        return is_bar_id(msg->u.frame.bar_id) && msg->u.frame.led_count <= STINGER_MAX_FRAME_LEDS
                   ? STINGER_OK
                   : STINGER_ERR_BAD_FIELD;
    case STINGER_MSG_CONFIG:
        return (is_bar_id(msg->u.config.bar_id) || msg->u.config.bar_id == STINGER_BAR_ALL) &&
                       is_known_key(msg->u.config.key)
                   ? STINGER_OK
                   : STINGER_ERR_BAD_FIELD;
    case STINGER_MSG_CONFIG_MAC:
        return is_known_key(msg->u.config_mac.key) ? STINGER_OK : STINGER_ERR_BAD_FIELD;
    case STINGER_MSG_HELLO:
        return msg->u.hello.bar_id <= STINGER_MAX_BAR_ID ? STINGER_OK : STINGER_ERR_BAD_FIELD;
    default:
        return STINGER_OK;
    }
}

/* Payload length for a message, or -1 for an unknown type. */
static int payload_size(uint8_t type, uint8_t led_count) {
    switch (type) {
    case STINGER_MSG_FRAME:
        return FRAME_FIXED_SIZE + 3 * led_count;
    case STINGER_MSG_SHOW:
        return SHOW_SIZE;
    case STINGER_MSG_CONFIG:
        return CONFIG_SIZE;
    case STINGER_MSG_PING:
    case STINGER_MSG_PONG:
        return PING_SIZE;
    case STINGER_MSG_CONFIG_MAC:
        return CONFIG_MAC_SIZE;
    case STINGER_MSG_HELLO:
        return HELLO_SIZE;
    case STINGER_MSG_STATS:
        return STATS_SIZE;
    default:
        return -1;
    }
}

int stinger_encode(const stinger_msg_t *msg, uint8_t *out, size_t cap) {
    uint8_t led_count = msg->type == STINGER_MSG_FRAME ? msg->u.frame.led_count : 0;
    int size = payload_size(msg->type, led_count);
    if (size < 0) {
        return STINGER_ERR_UNKNOWN_TYPE;
    }
    stinger_err_t err = check_fields(msg);
    if (err != STINGER_OK) {
        return err;
    }
    size_t total = STINGER_HEADER_SIZE + (size_t)size;
    if (total > cap) {
        return STINGER_ERR_NO_SPACE;
    }
    out[0] = msg->type;
    put_u16(out + 1, msg->seq);
    uint8_t *p = out + STINGER_HEADER_SIZE;
    switch (msg->type) {
    case STINGER_MSG_FRAME:
        p[0] = msg->u.frame.bar_id;
        p[1] = msg->u.frame.led_count;
        if (led_count > 0) {
            memcpy(p + 2, msg->u.frame.rgb, 3u * led_count);
        }
        break;
    case STINGER_MSG_SHOW:
        put_u16(p, msg->u.show.frame_seq);
        break;
    case STINGER_MSG_CONFIG:
        p[0] = msg->u.config.bar_id;
        p[1] = msg->u.config.key;
        put_u16(p + 2, msg->u.config.value);
        break;
    case STINGER_MSG_PING:
    case STINGER_MSG_PONG:
        put_u32(p, msg->u.ping.t_ms);
        break;
    case STINGER_MSG_CONFIG_MAC:
        memcpy(p, msg->u.config_mac.mac, STINGER_MAC_SIZE);
        p[6] = msg->u.config_mac.key;
        put_u16(p + 7, msg->u.config_mac.value);
        break;
    case STINGER_MSG_HELLO:
        p[0] = msg->u.hello.bar_id;
        memcpy(p + 1, msg->u.hello.mac, STINGER_MAC_SIZE);
        p[7] = msg->u.hello.led_count;
        put_u16(p + 8, msg->u.hello.fw_version);
        p[10] = (uint8_t)msg->u.hello.rssi;
        break;
    case STINGER_MSG_STATS:
        put_u32(p, msg->u.stats.rx_packets);
        put_u32(p + 4, msg->u.stats.crc_errors);
        put_u32(p + 8, msg->u.stats.tx_failures);
        break;
    default:
        break;
    }
    return (int)total;
}

stinger_err_t stinger_decode(const uint8_t *packet, size_t len, stinger_msg_t *msg) {
    if (len < STINGER_HEADER_SIZE) {
        return STINGER_ERR_TOO_SHORT;
    }
    const uint8_t *p = packet + STINGER_HEADER_SIZE;
    size_t n = len - STINGER_HEADER_SIZE;
    uint8_t type = packet[0];
    if (payload_size(type, 0) < 0) {
        return STINGER_ERR_UNKNOWN_TYPE;
    }
    if (type == STINGER_MSG_FRAME && n < FRAME_FIXED_SIZE) {
        return STINGER_ERR_BAD_LENGTH;
    }
    uint8_t led_count = type == STINGER_MSG_FRAME ? p[1] : 0;
    if (n != (size_t)payload_size(type, led_count)) {
        return STINGER_ERR_BAD_LENGTH;
    }
    memset(msg, 0, sizeof(*msg));
    msg->type = type;
    msg->seq = get_u16(packet + 1);
    switch (type) {
    case STINGER_MSG_FRAME:
        msg->u.frame.bar_id = p[0];
        msg->u.frame.led_count = led_count;
        msg->u.frame.rgb = p + 2;
        break;
    case STINGER_MSG_SHOW:
        msg->u.show.frame_seq = get_u16(p);
        break;
    case STINGER_MSG_CONFIG:
        msg->u.config.bar_id = p[0];
        msg->u.config.key = p[1];
        msg->u.config.value = get_u16(p + 2);
        break;
    case STINGER_MSG_PING:
    case STINGER_MSG_PONG:
        msg->u.ping.t_ms = get_u32(p);
        break;
    case STINGER_MSG_CONFIG_MAC:
        msg->u.config_mac.mac = p;
        msg->u.config_mac.key = p[6];
        msg->u.config_mac.value = get_u16(p + 7);
        break;
    case STINGER_MSG_HELLO:
        msg->u.hello.bar_id = p[0];
        msg->u.hello.mac = p + 1;
        msg->u.hello.led_count = p[7];
        msg->u.hello.fw_version = get_u16(p + 8);
        msg->u.hello.rssi = (int8_t)p[10];
        break;
    case STINGER_MSG_STATS:
        msg->u.stats.rx_packets = get_u32(p);
        msg->u.stats.crc_errors = get_u32(p + 4);
        msg->u.stats.tx_failures = get_u32(p + 8);
        break;
    default:
        break;
    }
    return check_fields(msg);
}

int stinger_wire_encode(const uint8_t *packet, size_t len, uint8_t *out, size_t cap) {
    if (len > STINGER_MAX_PACKET) {
        return STINGER_ERR_BAD_LENGTH;
    }
    uint8_t body[STINGER_MAX_PACKET + STINGER_CRC_SIZE];
    memcpy(body, packet, len);
    put_u16(body + len, stinger_crc16(packet, len));
    int n = stinger_cobs_encode(body, len + STINGER_CRC_SIZE, out, cap);
    if (n < 0) {
        return n;
    }
    if ((size_t)n >= cap) {
        return STINGER_ERR_NO_SPACE;
    }
    out[n] = 0;
    return n + 1;
}

void stinger_wire_decoder_init(stinger_wire_decoder_t *dec) {
    dec->len = 0;
    dec->overflow = false;
}

stinger_wire_status_t stinger_wire_feed(stinger_wire_decoder_t *dec, uint8_t byte,
                                        const uint8_t **packet, size_t *len, stinger_err_t *err) {
    if (byte != 0) {
        if (dec->len == sizeof(dec->buf)) {
            dec->overflow = true;
        } else if (!dec->overflow) {
            dec->buf[dec->len++] = byte;
        }
        return STINGER_WIRE_NONE;
    }
    size_t body_len = dec->len;
    bool overflow = dec->overflow;
    stinger_wire_decoder_init(dec);
    if (body_len == 0 && !overflow) {
        return STINGER_WIRE_NONE;
    }
    int n = overflow ? STINGER_ERR_FRAMING
                     : stinger_cobs_decode(dec->buf, body_len, dec->packet, sizeof(dec->packet));
    if (n < STINGER_HEADER_SIZE + STINGER_CRC_SIZE) {
        *err = STINGER_ERR_FRAMING;
        return STINGER_WIRE_ERROR;
    }
    size_t packet_len = (size_t)n - STINGER_CRC_SIZE;
    if (stinger_crc16(dec->packet, packet_len) != get_u16(dec->packet + packet_len)) {
        *err = STINGER_ERR_CRC;
        return STINGER_WIRE_ERROR;
    }
    *packet = dec->packet;
    *len = packet_len;
    return STINGER_WIRE_PACKET;
}
