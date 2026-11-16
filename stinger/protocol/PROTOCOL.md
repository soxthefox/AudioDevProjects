# Stinger protocol, version 1

This document is the normative definition of the bytes exchanged between the laptop, the gateway and the bars. The C codec (`stinger_proto.h/.c`), the Python codec (`stinger_proto/`) and the golden vectors (`vectors/vectors.json`) must all agree with it. If they disagree with this document, they are wrong.

## 1. Layers

```
laptop (bridge) ──serial link──▶ gateway ──radio link──▶ bars
                ◀──serial link──         ◀──radio link──
```

There is one **packet** format (section 3), used on every hop. Each link type wraps packets differently:

| Link | Framing | Integrity |
| --- | --- | --- |
| Serial (USB serial, or a TCP socket to the simulator, or a future wired bar link) | COBS, `0x00` delimiter (section 2) | CRC-16 per packet |
| Radio (ESP-NOW) | one packet per ESP-NOW frame | ESP-NOW's own MAC-layer FCS; no extra CRC |

A packet is never larger than **250 bytes** (the ESP-NOW payload limit), so any packet can cross any link unchanged.

## 2. Serial framing

To send a packet `P` on a serial link:

1. Compute `crc = CRC16(P)` (section 2.1).
2. Append `crc` as two bytes, **little-endian**: `P' = P || crc_lo || crc_hi`.
3. COBS-encode `P'` (section 2.2).
4. Write the encoded bytes followed by one `0x00` delimiter.

A receiver collects bytes up to each `0x00`, then:

- An empty frame (two delimiters in a row) is ignored and not counted. Senders may write a leading `0x00` to force a resync.
- A frame that is not valid COBS, or that decodes to fewer than 5 bytes (3 header + 2 CRC), is a **framing error**.
- A frame longer than the maximum encoded size (251 bytes before the delimiter) is a **framing error**; the receiver discards bytes until the next delimiter.
- If the CRC over the decoded bytes (excluding the last two) does not match the last two bytes, it is a **CRC error**.

Framing and CRC errors are dropped silently and counted (both count as `crc_errors` in STATS).

### 2.1 CRC

CRC-16/CCITT-FALSE (also catalogued as CRC-16/IBM-3740): polynomial `0x1021`, initial value `0xFFFF`, no input or output reflection, no final XOR. Check value: `CRC16("123456789") = 0x29B1`. Python's `binascii.crc_hqx(data, 0xFFFF)` computes the same function.

### 2.2 COBS

Consistent Overhead Byte Stuffing as published by Cheshire and Baker (1999), without the optional zero-pair elimination. The encoded output never contains `0x00`. A 250-byte input (largest packet plus CRC) encodes to at most 251 bytes.

## 3. Packet

All multi-byte integers are **little-endian**. Signed values are two's complement.

```
offset  size  field
0       1     type     message type (section 4)
1       2     seq      sequence number, u16
3       n     payload  depends on type; length is fixed per type except FRAME
```

`seq` is chosen by whoever creates the packet: the sender adds 1 for each packet it creates on that link and wraps from `0xFFFF` to `0x0000`. Receivers use it for diagnostics only and must accept any value, in any order.

Decoding is **strict**. A packet is rejected, and dropped, with the first of these errors that applies:

1. `TOO_SHORT`: fewer than 3 bytes.
2. `UNKNOWN_TYPE`: `type` is not in section 4.
3. `BAD_LENGTH`: the payload length is not exactly what the type requires (for FRAME: at least 2, and then exactly `2 + 3 × led_count`).
4. `BAD_FIELD`: a field is outside the ranges in section 4.1.

## 4. Messages

Direction: **L** = laptop, **G** = gateway, **B** = bar.

| Type | Name | Direction | Payload | Payload bytes |
| --- | --- | --- | --- | --- |
| `0x01` | FRAME | L→G→B | `bar_id u8, led_count u8, rgb[3 × led_count]` | 2 + 3 × led_count |
| `0x02` | SHOW | L→G→B | `frame_seq u16` | 2 |
| `0x03` | CONFIG | L→G→B | `bar_id u8, key u8, value u16` | 4 |
| `0x04` | PING | L→G | `t_ms u32` | 4 |
| `0x05` | CONFIG_MAC | L→G→B | `mac[6], key u8, value u16` | 9 |
| `0x81` | HELLO | B→G→L | `bar_id u8, mac[6], led_count u8, fw_version u16, rssi i8` | 11 |
| `0x82` | PONG | G→L | `t_ms u32` | 4 |
| `0x83` | STATS | G→L | `rx_packets u32, crc_errors u32, tx_failures u32` | 12 |

### 4.1 Field ranges

| Field | Valid values |
| --- | --- |
| `bar_id` in FRAME | 1–16 |
| `bar_id` in CONFIG | 1–16, or `0xFF` = every bar |
| `bar_id` in HELLO | 0 (unassigned) or 1–16 |
| `led_count` | 0–81. `81 = ⌊(250 − 5) / 3⌋`, the most that fits one 250-byte packet |
| `mac` in CONFIG_MAC | any; `FF:FF:FF:FF:FF:FF` = every bar |
| `key` | a key from section 4.4 |
| `fw_version` | `major << 8 | minor` |
| `rssi` | dBm as reported by the gateway's radio; `0` = unknown |

### 4.2 FRAME and SHOW

- `rgb` is `led_count` pixels of 3 bytes each, in **R, G, B** order, pixel 0 first. Bars reorder to their strip's wiring (WS2812B is GRB) themselves.
- A bar keeps only the latest FRAME addressed to its own `bar_id` in a pending buffer. FRAMEs for other IDs are ignored. An unassigned bar (ID 0) ignores every FRAME.
- On SHOW, every bar copies its pending buffer to its LEDs. If a bar received no new FRAME since the previous SHOW it keeps showing what it has (latest frame wins; a lost FRAME is replaced on the next frame).
- If a FRAME carries fewer pixels than the bar has, the remaining pixels are black. Extra pixels are ignored.
- `frame_seq` is the sender's frame counter, +1 per frame, wrapping. Receivers use it to count dropped frames only.
- The laptop sends one FRAME per bar, then one SHOW, for every frame.

### 4.3 CONFIG and CONFIG_MAC

CONFIG addresses bars by `bar_id`; CONFIG_MAC addresses one bar by MAC, which is how unassigned bars are reached. A bar applies a CONFIG whose `bar_id` is its own or `0xFF`, and a CONFIG_MAC whose `mac` is its own or all-`FF`. It ignores the rest. Settings other than the ID are kept in RAM and reset on reboot.

### 4.4 Config keys

| Key | Name | Value | Bar behaviour |
| --- | --- | --- | --- |
| `0x01` | BRIGHTNESS_CAP | 0–255, a scale applied to every channel (255 = full scale) | Sets the runtime cap, clamped to the firmware's hard limit. The laptop can lower the brightness but never raise it above the hard limit. |
| `0x02` | FAILSAFE_TIMEOUT_MS | milliseconds | Sets the failsafe timeout (section 6), clamped to at least 100 ms. |
| `0x03` | ASSIGN_ID | 0–16 | Stores the new ID in non-volatile memory and uses it at once. 0 unassigns. Values above 16 are ignored. |

### 4.5 PING, PONG, STATS

- The gateway answers each PING with a PONG carrying the same `t_ms`. The laptop computes the round trip from its own clock.
- The gateway sends STATS every second. Counters are totals since the gateway booted and wrap at 2³²:
  - `rx_packets`: valid packets received on the serial link.
  - `crc_errors`: framing plus CRC errors on the serial link.
  - `tx_failures`: radio sends that failed (broadcast queue full, or unicast not acknowledged after all retries).

### 4.6 HELLO

Each bar broadcasts HELLO every **1 s**, with its current `bar_id` (0 if unassigned), its MAC, LED count and firmware version, and `rssi = 0`. The gateway fills in `rssi` from the received radio frame and forwards it to the laptop.

## 5. Gateway forwarding rules

| Packet from laptop | Gateway action |
| --- | --- |
| FRAME, SHOW | Radio **broadcast**, no acknowledgement, forwarded byte for byte. |
| CONFIG | Radio **unicast** to the MAC last seen in a HELLO with that `bar_id` (for `0xFF`, to every bar seen in the last 3 s), with ESP-NOW acknowledgement and up to **3 retries**. Unknown `bar_id` counts as a `tx_failure`. |
| CONFIG_MAC | Radio unicast to `mac`, acknowledged, up to 3 retries. For all-`FF`, broadcast once. |
| PING | Answered with PONG, not forwarded. |
| HELLO, PONG, STATS | Not expected from the laptop; dropped. |

| Packet from radio | Gateway action |
| --- | --- |
| HELLO | Remember `bar_id → mac`, set `rssi`, re-encode with the gateway's own `seq`, send to laptop. |
| anything else | Dropped. |

## 6. Bar behaviour

- **Identity.** The bar's ID (0–16) is kept in non-volatile memory. With ID 0 it shows a slow, dim white breathing pattern so it can be seen and assigned.
- **Brightness cap.** Every output channel is scaled by `cap / 255` after the frame arrives and before it reaches the LEDs. The cap is `min(runtime_cap, hard_limit)`; the hard limit is a firmware build constant (default 178 ≈ 70 %). An independent current limiter (5 V, 3 A) runs in the LED driver as a second guard.
- **Failsafe.** If no FRAME addressed to the bar arrives for the failsafe timeout (default 2 s), the bar fades its current output linearly to black over 500 ms, then stays black. The next FRAME + SHOW restores normal output at once. A bar that has never received a frame shows black.

## 7. Laptop behaviour

- Marks a bar **offline** after 3 s without a HELLO from its MAC, and online again on the next HELLO.
- Sends frames at no more than the target frame rate (default 40 fps). If its input stops, it keeps re-sending the last frame for up to 1 s, then stops, letting the bars' failsafe take over.

## 8. Sizes

| Item | Bytes |
| --- | --- |
| Packet header | 3 |
| Largest packet (FRAME, 81 LEDs) | 248 |
| FRAME, 60 LEDs | 185 |
| Packet + CRC, largest | 250 |
| Encoded serial frame, largest (with delimiter) | 252 |

At 921600 baud (8N1, 92 160 bytes/s) one 10-bar frame at 60 LEDs is 10 × (185 + 2 + 1 + 1) + (5 + 2 + 1 + 1) = 1899 bytes, about 20.6 ms of line time, inside the 25 ms frame budget at 40 fps. At 2 000 000 baud it is about 9.5 ms.

## 9. Golden vectors

`vectors/vectors.json` lists encoded byte strings together with their meaning. Both codecs' tests check every vector in both directions (decode the bytes, encode the meaning). `vectors/golden_vectors.h` is the same data for the C tests, generated by `vectors/gen_c_vectors.py`; a Python test fails if it is stale.
