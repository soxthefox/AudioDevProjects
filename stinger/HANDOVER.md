# Light Installation – Stinger: Layer 1 Handover

This is the plan for the software-only layer of the project: everything that can be built, run and tested without physical hardware. Read it fully before writing code. Record any decision that departs from it in `DECISIONS.md`, with the reason.

## 1. What the project is

A client installation of 10 LED light bars for a dance floor, controlled live from a laptop.

- Each bar: 1 m of WS2812B 5V strip (60 LEDs), an ESP32 DevKit (30-pin), a 74HCT125 level shifter, its own 5V power.
- One extra ESP32, the **gateway**, plugs into the laptop over USB serial and relays frames to the bars over **ESP-NOW** (no Wi-Fi router).
- Bars **announce themselves** when powered on and appear in the control software, like game controllers.
- Control software for the first delivery is **QLC+ or LedFx**, which send frames as **Art-Net** (or sACN/E1.31). A laptop-side **bridge** turns those into our serial protocol for the gateway.
- A custom control app may come later; it is **out of scope** here, but everything below must make it easy to add.

Data path:

```
QLC+ / LedFx ──Art-Net/sACN (UDP, localhost)──▶ bridge ──USB serial──▶ gateway ESP32 ──ESP-NOW──▶ bar ESP32 ×10 ──▶ WS2812B
                                                    │
                                                    └──(sim mode)──▶ simulator: virtual gateway + 10 virtual bars in a browser
```

## 2. Scope of Layer 1

**In scope**

1. A written **protocol spec** plus shared codecs (C for firmware, Python for laptop side) and golden test vectors.
2. **Simulator**: a fake gateway and 10 virtual bars, shown in a browser view.
3. **Bridge**: Art-Net/sACN in, our protocol out, to either the real serial port or the simulator.
4. **Gateway firmware** and **bar firmware**, which must compile and pass host-side tests.
5. A **test-pattern tool** that sends Art-Net without QLC+.
6. Docs: run instructions and how to configure QLC+ and LedFx.

**Out of scope**

- Flashing or testing real boards (that is Layer 2, done by the human).
- The custom control app.
- PCB design.

## 3. Fixed hardware facts

| Item | Value |
| --- | --- |
| Bars | 10 (design for up to 16) |
| LEDs per bar | 60, WS2812B, GRB order, 5V |
| Bytes per bar frame | 180 (60 × RGB) |
| MCU | ESP32 DevKit, 30-pin, ESP32-WROOM-32D, CH340C USB serial |
| Data pin | GPIO16 (make it a build flag) |
| Brightness cap | default 70% of full scale, hard limit in firmware |
| Power limit per bar | 5V, 3 A (use FastLED's power limiter as a second guard) |
| Target frame rate | 40 fps (25 ms per frame) |
| ESP-NOW payload limit | 250 bytes per packet |

These are build-time constants or config values, never magic numbers scattered in code.

## 4. Protocol (draft — finalise it in `protocol/PROTOCOL.md` first)

### 4.1 Laptop ↔ gateway (USB serial)

- Baud: **921600** default, configurable (try 2,000,000 later; the CH340C supports it).
- Framing: **COBS** encoding with a `0x00` delimiter, and a **CRC-16/CCITT** over each decoded packet. Bad CRC = drop silently and count it.
- Every packet starts with: `type (u8) | seq (u16, little-endian) | payload`.

Message types (laptop → gateway unless noted):

| Type | Name | Payload | Notes |
| --- | --- | --- | --- |
| 0x01 | FRAME | `bar_id u8, led_count u8, rgb[led_count×3]` | One per bar per frame |
| 0x02 | SHOW | `frame_seq u16` | Sent after all FRAMEs for a frame; bars latch together |
| 0x03 | CONFIG | `bar_id u8 (0xFF = all), key u8, value u16` | brightness cap, failsafe timeout, assign ID |
| 0x04 | PING | `t_ms u32` | gateway answers PONG |
| 0x81 | HELLO (gateway → laptop) | `bar_id u8, mac[6], led_count u8, fw_version u16, rssi i8` | forwarded from each bar's broadcast |
| 0x82 | PONG (gateway → laptop) | `t_ms u32` | for round-trip latency |
| 0x83 | STATS (gateway → laptop) | counters: rx packets, CRC errors, ESP-NOW send failures | every 1 s |

### 4.2 Gateway ↔ bars (ESP-NOW)

- 180 bytes of RGB plus a small header fits in **one ESP-NOW packet per bar**. Keep it that way while bars have ≤ 76 LEDs; above that, add an `offset` field and split.
- FRAME and SHOW go out as **broadcast**, no acknowledgement. Latest frame wins; a lost frame is replaced 25 ms later.
- CONFIG goes out **unicast** to the bar's MAC, with ESP-NOW acknowledgement and up to 3 retries.
- Bars **ignore FRAMEs not addressed to their bar_id** and only display on SHOW, so all bars change on the same frame.

### 4.3 Bar identity and discovery

- Each bar keeps its `bar_id` (1–16) in NVS. Unassigned bars use `0` and are only addressable by MAC.
- Bars broadcast **HELLO every 1 s**. The laptop side marks a bar **offline after 3 s** without HELLO.
- The laptop assigns IDs with CONFIG `assign ID` sent to a MAC.

### 4.4 Failsafe

- If a bar receives no frame for **2 s** (configurable), it fades to black over 500 ms.
- Brightness cap is applied **in the bar firmware**, after any value the laptop sends. The laptop cannot exceed it.

## 5. Repository layout

```
protocol/      PROTOCOL.md, stinger_proto.h/.c (C codec), stinger_proto/ (Python package), vectors/ (golden test vectors)
firmware/
  gateway/     PlatformIO project (esp32dev, Arduino framework)
  bar/         PlatformIO project (esp32dev, Arduino framework, FastLED)
  common/      transport interface + shared helpers
bridge/        Python: Art-Net/sACN in → protocol out (serial or sim)
sim/           simulator: virtual gateway + virtual bars + browser view
tools/         test-pattern sender, latency probe
tests/         pytest suites; firmware native tests live under each PlatformIO project (test/)
docs/          QLC+ and LedFx setup guides
DECISIONS.md   any departure from this plan, with reasons
```

Firmware must isolate transport behind an interface (`send`, `onReceive`) so the same bar logic can later run over a plain wired serial link instead of ESP-NOW.

## 6. Component specs

### 6.1 Protocol codecs

- C codec: no dynamic allocation, usable on ESP32 and in PlatformIO `native` tests.
- Python codec: same encode/decode, typed, no third-party dependencies beyond the standard library.
- Golden vectors: a set of encoded byte strings with their decoded meaning, checked by **both** codecs.

### 6.2 Simulator (`sim/`)

- Implements a **virtual gateway** that speaks exactly the serial protocol, over a TCP socket (or a pseudo-terminal), so the bridge cannot tell it from real hardware.
- Contains **10 virtual bars** that apply the same rules as the firmware: bar_id filtering, latch on SHOW, brightness cap, failsafe fade, HELLO every 1 s.
- **Browser view** (single HTML page, canvas, WebSocket from the sim): 10 bars of 60 pixels stacked like the client's DJ booth, with labels, live fps, frame latency, and dropped-frame counts.
- Controls in the view: **unplug/replug** each virtual bar, inject packet loss (0–30%), and change the brightness cap.
- Should be shareable as a client preview: one command starts it and prints the URL.

### 6.3 Bridge (`bridge/`)

- Inputs: **Art-Net** on UDP 6454 and **sACN/E1.31** on UDP 5568. Optional later: DDP on UDP 4048 (LedFx supports it).
- Mapping, from a YAML config: **one universe per bar** by default (universe 0 → bar 1 … universe 9 → bar 10, channels 1–180 = 60 RGB pixels). Allow overrides.
- Outputs: `--out serial:/dev/ttyUSB0` or `--out sim:localhost:PORT`.
- Assembles a full frame (all bars) and sends FRAMEs then SHOW, paced to the target fps. If input stops, keep sending the last frame for up to 1 s, then stop.
- Prints the live bar list (from HELLO), online or offline status, and gateway STATS.
- Added latency from Art-Net in to SHOW out: **under 10 ms** on a laptop.

### 6.4 Gateway firmware (`firmware/gateway/`)

- Serial in → validate CRC → forward over ESP-NOW per section 4.2.
- ESP-NOW in (HELLO) → forward to serial.
- PING/PONG and 1 s STATS.
- Must compile for `esp32dev`; protocol parsing covered by native tests.

### 6.5 Bar firmware (`firmware/bar/`)

- ESP-NOW receive → filter by bar_id → buffer → display on SHOW via FastLED on GPIO16.
- Brightness cap, FastLED power limit (5V, 3 A), failsafe fade, HELLO every 1 s, NVS-stored ID, CONFIG handling.
- On boot with no ID: slow white breathing at low brightness, so an unassigned bar is visible.
- Must compile for `esp32dev`; logic covered by native tests (filtering, latch, cap, failsafe timing).

### 6.6 Tools (`tools/`)

- `patterns`: sends Art-Net test patterns (solid, chase, rainbow, strobe, per-bar ID flash) to the bridge without QLC+.
- `latency`: measures PING/PONG round trip and Art-Net-to-SHOW delay.

### 6.7 Wokwi (optional)

- Provide a Wokwi `diagram.json` for one bar ESP32 with a 60-LED strip, driven by a serial test input.
- If `WOKWI_CLI_TOKEN` is set, run it headless with the Wokwi CLI in the test script; if not, skip with a clear message.
- Do not depend on multi-board ESP-NOW simulation; treat the radio as covered by the simulator and by Layer 2.

## 7. Build order and acceptance criteria

Work in this order and commit at each milestone.

| # | Milestone | Done when |
| --- | --- | --- |
| M1 | Protocol | `PROTOCOL.md` complete; C and Python codecs pass all golden vectors; round-trip and corrupt-input tests pass |
| M2 | Simulator | One command starts it; browser shows 10 × 60 bars; unplugging a bar removes it from the bridge's list within 3 s and replugging restores it |
| M3 | Bridge + tools | `tools/patterns` → bridge → sim shows each pattern; QLC+ sending Art-Net to universes 0–9 shows correctly in the sim; added latency under 10 ms |
| M4 | Gateway firmware | Compiles for `esp32dev`; native tests for parsing, CRC and forwarding logic pass |
| M5 | Bar firmware | Compiles for `esp32dev`; native tests for filtering, latch on SHOW, brightness cap and failsafe pass |
| M6 | Wokwi (optional) | Single-bar simulation runs headless when a token is present |
| M7 | Docs | README with setup and run steps; `docs/` guides for QLC+ (10 universes, 60 RGB pixels each) and LedFx |

## 8. Engineering rules

- Code quality matters as much as working code: small modules, clear names, no dead code.
- Python 3.11+, type hints, `ruff` for lint and format, `pytest` for tests.
- C/C++: `clang-format`, warnings as errors in native test builds.
- One command runs everything testable: `make test` (or a script) runs Python tests, PlatformIO native tests and firmware builds.
- No secrets in the repo. Wokwi token comes from the environment only.
- Never flash or touch serial devices automatically; the human does hardware steps.
- When something in this plan is wrong or unclear, decide, write it in `DECISIONS.md`, and keep going. Only stop for something irreversible.

## 9. Open questions for the human

- Final bar length and LED density (plan assumes 1 m, 60 LEDs/m).
- QLC+ or LedFx for the first delivery (support both inputs until decided).
- Whether 2,000,000 baud is stable on the real CH340C boards.
- Physical arrangement of bars for the simulator view (default: 10 stacked horizontal bars).
