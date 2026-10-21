# Decisions

Departures from, or clarifications of, `HANDOVER.md`. Newest last. Each entry says what was decided and why.

## D1. Project lives in `stinger/`, not the repository root

The repository (`AudioDevProjects`) is a monorepo whose convention is one self-contained project per top-level folder. The handover's "repo root" layout (`protocol/`, `firmware/`, …) is therefore placed under `stinger/`, and `HANDOVER.md` sits at `stinger/HANDOVER.md`. All paths in the handover are relative to `stinger/`.

## D2. Python 3.14 from Homebrew in a project virtualenv

The plan asks for Python 3.11+. The machine's default `python3` is 3.9, so `make` creates `stinger/.venv` from the newest `python3.1x` it finds (3.14 here) and installs dev tools there, including a current PlatformIO. Nothing is installed system-wide.

## D3. One packet format on every link; framing differs per link

The serial link wraps packets in COBS + CRC-16. The radio link carries the same packet bytes with no extra CRC, because ESP-NOW already checks each frame. The gateway can therefore forward FRAME, SHOW and CONFIG byte for byte, and a future wired bar link only needs the serial framing. Every packet is at most 250 bytes so it fits one ESP-NOW frame.

## D4. CRC is CRC-16/CCITT-FALSE, appended little-endian

"CRC-16/CCITT" names several variants. We use poly 0x1021, init 0xFFFF, no reflection (check value 0x29B1), which matches Python's `binascii.crc_hqx(data, 0xFFFF)` and gives the tests an independent reference. It is appended little-endian, like every other integer.

## D5. A FRAME carries up to 81 LEDs, not 76

With a 5-byte header (type, seq, bar_id, led_count), `⌊(250 − 5) / 3⌋ = 81` LEDs fit one ESP-NOW packet. The plan's 76 assumed a bigger header. Bars longer than 81 LEDs will need the `offset` field the plan mentions; that is a protocol version 2 change.

## D6. New message CONFIG_MAC (0x05) for addressing by MAC

The plan says IDs are assigned "with CONFIG `assign ID` sent to a MAC", but CONFIG's payload addresses by `bar_id`. Unassigned bars all have ID 0, so a MAC address must travel in the packet. CONFIG_MAC is `mac[6], key, value`. The MAC is in the packet, not just in the ESP-NOW header, so the same bar logic works on a wired link that has no MACs.

## D7. Bars send HELLO with `rssi = 0`; the gateway fills it in

Only the gateway knows the received signal strength. The gateway patches `rssi` and re-encodes HELLO with its own `seq`. It also keeps a `bar_id → MAC` table from HELLOs, which it needs to unicast CONFIG by ID.

## D8. Strict decoding, with a fixed error order

Wrong payload lengths, unknown types, unknown config keys and out-of-range IDs are rejected rather than tolerated. Both codecs report the first error from the list in PROTOCOL.md section 3, so the golden vectors can name an exact error for every invalid input.

## D9. STATS `crc_errors` counts framing errors too; third counter is `tx_failures`

A bad COBS frame and a bad CRC are both "corrupt serial input", and the laptop only needs to see that the link is noisy. `tx_failures` counts all radio send failures (broadcast queue full, unicast not acknowledged after retries), not only ESP-NOW-specific ones, so the name still fits a wired link.

## D10. Brightness cap is an 8-bit scale; the laptop can only lower it

BRIGHTNESS_CAP takes 0–255 (a scale applied to every channel). Each bar applies `min(runtime_cap, hard_limit)`, where the hard limit is a build flag (default 178 ≈ 70 %). The runtime cap starts at the hard limit.

## D11. Failsafe is measured from the last FRAME addressed to this bar

"No frame for 2 s" is read as no FRAME for this bar's ID; SHOWs alone do not keep a bar lit. A bar that has never received a frame shows black (unless unassigned, which breathes).

## D12. Golden vectors come from an independent one-off script; JSON is the source of truth

`vectors.json` was produced once by a script written separately from both codecs and checked against the published COBS examples and `binascii.crc_hqx`. It is now edited by hand. The C tests read the same data through `golden_vectors.h`, which `gen_c_vectors.py` generates by data conversion only; a Python test fails if the header is stale.

## D13. The C codec's tests live in the gateway project's native environment

The handover puts native tests under each PlatformIO project. The gateway is the main parser of serial input, so `firmware/gateway/test/test_protocol` holds the golden-vector, round-trip and bit-flip tests for the C codec. The protocol folder is a PlatformIO library (`protocol/library.json`) that both firmware projects link via `symlink://`.

## D14. Hardware and timing facts live in `config/installation.yaml`

The laptop side reads one YAML file for bar count, LEDs per bar, fps, baud, brightness hard limit and protocol timings. The firmware gets the same values as build flags; a test checks the two agree, so there is one documented number per fact and no copy can drift silently.

## D15. Simulator: virtual gateway over TCP, with the serial line speed simulated

The virtual gateway listens on TCP (default port 7777) and speaks the exact serial wire format, so the bridge's `--out sim:HOST:PORT` differs from `--out serial:DEVICE` only in the transport. The simulator holds each incoming serial frame until a real 8N1 line at the configured baud would have delivered it, so the view's latency and fps show the real link budget: one 10-bar frame at 921600 baud takes about 20.6 ms of the 25 ms frame. `--baud 0` turns this off. A new TCP client replaces the old one, as if the bridge had reopened the port.

## D16. Bar behaviour is pinned by shared scenarios, not by sharing code

The simulator's virtual bar is a Python port of the firmware's bar logic, with all-integer maths so both produce identical pixels. `tests/bar_scenarios.json` (filtering, latch on SHOW, cap, failsafe timing, breathing, ID assignment, HELLO timing) runs against the virtual bar now and against the firmware's native tests in M5. Sharing the C code via a compiled extension was rejected because it would make the client preview depend on a C toolchain.

## D17. Unplugged bars stay in the bridge's list, marked OFFLINE

"Removes it from the bridge's list" is implemented as moving it out of the online set: the operator still sees which bar dropped and for how long. Code that sends frames uses only the online set. "Unplug" in the simulator cuts the bar's power, so replugging reboots it: the ID survives (as in NVS), the runtime brightness cap and failsafe timeout reset, and it sends HELLO at once.

## D18. Simulator view metrics

Per bar: fps is SHOWs latched with a new FRAME in the last second; dropped counts SHOWs missed (frame_seq gaps up to 100; larger jumps are a resync) plus SHOWs that arrived without this bar's FRAME. Latency is one figure for the whole frame: from the first serial byte of a frame reaching the gateway to the SHOW latch, averaged over the last 40 frames.

## D19. Bridge listens on 127.0.0.1 by default

The plan says Art-Net/sACN arrive over UDP on localhost. Binding the specific loopback address (with `SO_REUSEADDR`/`SO_REUSEPORT`) also lets the bridge share port 6454 with QLC+, which binds it on all interfaces for its own Art-Net input: unicast to 127.0.0.1 goes to the more specific bind. `--listen 0.0.0.0` accepts input from the network, and only then does the bridge join the sACN multicast groups. Over loopback, configure senders for unicast to 127.0.0.1.

## D20. sACN universes default to 1–10

sACN has no universe 0, so the default mapping is Art-Net universe `N − 1` and sACN universe `N` for bar N. Both bases and per-bar overrides are in `config/bridge.yaml`.

## D21. When the bridge sends a frame

A frame goes out as soon as every bar's universe has new data, or on ArtSync / sACN sync; with only some universes updated, 5 ms after the first one. A token bucket (40 fps average, bursts of 2) protects the serial link. A strict "25 ms since the last send" rule was tried first and measured badly: the wait carried each frame's delay into the next, giving a 6–7 ms median. When input stops, the last frame is repeated every 2 frame intervals for 1 s, then sending stops. Repeats start only after 2 intervals of silence so they never race live input. The bridge sends a FRAME for every configured bar whether or not it is online, because a bar may come back at any time and its FRAME costs the same airtime as a broadcast.

## D22. Python CRC uses `binascii.crc_hqx`

The pure-Python bit loop took 0.18 ms per bar, about 2 ms per frame, a fifth of the 10 ms budget. `binascii.crc_hqx(data, 0xFFFF)` is the same CRC in C and is still standard library. The golden check value still tests it.

## D23. Latency is measured from the first input packet of a frame to its SHOW

`stinger-latency artnet` runs the bridge in-process between a local Art-Net sender and a stand-in gateway, so it measures the bridge's added delay without hardware. Measured here: median 2.4 ms, p99 5.4 ms. `tests/test_bridge_e2e.py` fails if p99 reaches 10 ms. The bridge reports its own p50/p99 live.

## D24. No ArtPollReply

The bridge does not answer ArtPoll. QLC+ and LedFx both send to a configured IP without discovery. If a controller needs to discover the bridge, adding ArtPollReply is a small change.

## D25. Runtime dependencies: PyYAML and pyserial

The protocol codec stays standard library only. The bridge and tools use PyYAML (the YAML configs the plan asks for) and pyserial (imported only when `--out serial:` is used).

## D26. Firmware platform: `espressif32@6.9.0` (Arduino core 2.0.17, ESP-IDF 4.4), pinned

This is the newest official PlatformIO ESP32 platform and the one FastLED is most tested on. ESP-IDF 4.4's ESP-NOW receive callback carries no RSSI, so the gateway reports `rssi = 0` (unknown, which PROTOCOL.md allows). The transport already reads RSSI under ESP-IDF 5 (`#if ESP_IDF_VERSION_MAJOR >= 5`); moving to Arduino core 3 (the community `pioarduino` platform) would turn it on. That is a Layer 2 choice, after FastLED is checked on core 3.

## D27. ESP-NOW frames at 6 Mbps, as a build flag

At the ESP-NOW default of 1 Mbps, one 185-byte FRAME takes about 1.7 ms of airtime, so 10 bars use about 17 ms of every 25 ms frame, leaving little room for retries or other traffic. `STINGER_ESPNOW_PHY_RATE=WIFI_PHY_RATE_6M` (robust OFDM) cuts that to about 3 ms. Range and loss at this rate must be checked on the real floor in Layer 2; the flag makes it a one-line change.

## D28. Unicast sends block until acknowledged (up to 50 ms)

ESP-NOW reports delivery asynchronously. The transport waits on a semaphore for the send callback, so the gateway's retry loop stays simple and testable. A CONFIG to an absent bar can hold the loop for 4 × 50 ms; the 4 KB serial receive buffer covers about 44 ms at 921600 baud, so frames may be dropped while an operator sends CONFIG to a missing bar. CONFIG is a rare manual action and dropped frames are already tolerated, so this is accepted.

## D29. Firmware facts are build flags with no defaults

`src/firmware_config.h` stops the build with `#error` if a flag is missing, so there is no second copy of any number to drift. Logic classes take plain config structs and never read macros, which keeps them testable natively. `tests/test_config.py` compares the flags with `config/installation.yaml`. Native test builds use `-Werror`; the ESP32 build uses `-Wall -Wextra` without `-Werror` because the Arduino core's own headers are not warning-clean across versions. It currently builds with no warnings from this project's code.

## D30. The Transport interface also reports link counters

The gateway's STATS needs serial `rx_packets` and `crc_errors`, which only the serial framing layer knows. `Transport::counters()` exposes them for every link type, so gateway logic depends only on the interface.

## D31. Bar: cap applied in our code, FastLED's power limiter as the second guard

`BarLogic::render` produces final pixel values (`value × cap × fade / 255²`, integer maths), and FastLED's global brightness stays at 255. This keeps the cap in tested, portable code, and identical to the simulator. `FastLED.setMaxPowerInVoltsAndMilliamps(5, 3000)` runs underneath as the independent current limit. FastLED is pinned to 3.9.4. The strip is only re-sent when the pixels actually change.

## D32. Unassigned bars breathe on a triangle wave

The plan asks for "slow white breathing at low brightness". A triangle wave (0 → 24 → 0 over 4 s, then capped) keeps the maths integer, so firmware and simulator match exactly; a sine would need floats and could differ by one step. A stored ID above 16 (corrupt NVS) boots as unassigned.

## D33. The firmware's bar tests replay the simulator's scenarios

`tests/gen_bar_scenarios.py` turns `tests/bar_scenarios.json` into `firmware/bar/test/test_bar/bar_scenarios.h`, encoding messages with the Python codec (already pinned by the golden vectors). The same 13 scenarios pass on `VirtualBar` (Python) and `BarLogic` (C++). A Python test fails if the header is stale. Firmware-only behaviour (ID saved to and loaded from storage, `millis()` wrap-around, HELLO sent by `loop()`) has its own native tests.

## D34. Swapping the bar to a wired link is a change to `main.cpp` only

`BarLogic` takes a `Transport&`. A wired bar would construct `SerialTransport` over an `ArduinoStream` on a UART instead of `EspNowTransport`; CONFIG_MAC still works because MACs travel inside packets (D6).
