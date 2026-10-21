# Light Installation – Stinger

Ten LED bars for a dance floor, played live from a laptop. Each bar is an ESP32 driving 60 WS2812B pixels. A gateway ESP32 on the laptop's USB port relays frames to the bars over ESP-NOW. QLC+ or LedFx send Art-Net or sACN to a bridge on the laptop, which speaks the Stinger protocol to the gateway, or to a browser simulator when no hardware is attached.

```
QLC+ / LedFx ──Art-Net/sACN──▶ stinger-bridge ──USB serial──▶ gateway ──ESP-NOW──▶ 10 bars
                                     │
                                     └──TCP──▶ stinger-sim (virtual gateway + 10 virtual bars, browser view)
```

This folder is Layer 1: everything that runs and is tested without hardware. The plan is [HANDOVER.md](HANDOVER.md). Every departure from it, with the reason, is in [DECISIONS.md](DECISIONS.md).

## Setup

Needs Python 3.11+ (`python3.11` … `python3.14` on PATH) and `make`. PlatformIO is installed into the project's virtualenv; the first firmware build downloads the ESP32 toolchain (about 1 GB, into `~/.platformio`).

```bash
make venv
```

## Run

| What | Command |
| --- | --- |
| Simulator (prints the URL, default http://localhost:8765) | `make sim` |
| Bridge into the simulator | `.venv/bin/stinger-bridge --out sim:localhost:7777` |
| Bridge into the real gateway | `.venv/bin/stinger-bridge --out serial:/dev/cu.usbserial-XXXX` |
| Test patterns, no QLC+ needed | `.venv/bin/stinger-patterns rainbow` (also `solid`, `chase`, `strobe`, `id-flash`; `--sacn` for E1.31) |
| Gateway round trip | `.venv/bin/stinger-latency ping --out sim:localhost:7777` |
| Bridge Art-Net-to-SHOW delay | `.venv/bin/stinger-latency artnet` |
| List bars, assign IDs, set cap | `.venv/bin/stinger-config --out sim:localhost:7777 list` (also `assign`, `brightness`, `failsafe`) |

A full simulated show is three terminals: `make sim`, the bridge, then `stinger-patterns` or QLC+/LedFx. In the simulator view you can unplug bars, add packet loss and lower the brightness cap. `make sim ARGS="--host 0.0.0.0"` shares the view on the local network for a client preview.

Controller setup: [QLC+](docs/qlcplus.md) · [LedFx](docs/ledfx.md).

## Test

```bash
make test
```

This runs ruff and clang-format checks, the Python tests (codec, simulator, bridge, tools, end to end), the PlatformIO native tests (C codec, gateway logic, bar logic), both ESP32 firmware builds, and the single-bar Wokwi simulation, which is skipped unless `WOKWI_CLI_TOKEN` is set. Nothing in `make test` flashes a board or opens a serial port.

## Layout

| Path | What |
| --- | --- |
| `protocol/` | [PROTOCOL.md](protocol/PROTOCOL.md) (normative), C codec, Python codec `stinger_proto`, golden vectors |
| `config/` | `installation.yaml` (bar count, LEDs, fps, baud, brightness limit, timings), `bridge.yaml` (universe mapping) |
| `sim/` | `stinger-sim`: virtual gateway on TCP, virtual bars, browser view |
| `bridge/` | `stinger-bridge`: Art-Net/sACN in, protocol out, live bar list |
| `tools/` | `stinger-patterns`, `stinger-latency`, `stinger-config` |
| `firmware/common/` | `Transport` interface (`send`/`onReceive`/`poll`), serial and ESP-NOW implementations |
| `firmware/gateway/`, `firmware/bar/` | PlatformIO projects; logic in `lib/`, native tests in `test/` |
| `firmware/bar/wokwi/` | Wokwi diagram and headless run script for one bar |
| `tests/` | pytest suites; `bar_scenarios.json` is shared by the simulator and the bar firmware tests |
| `docs/` | QLC+ and LedFx guides |

Hardware facts live in two places only: `config/installation.yaml` for the laptop side, and build flags in each `platformio.ini` for the firmware. `tests/test_config.py` fails if they disagree. To change the LED count, for example, edit both and run `make test`.

## Flashing (Layer 2, done by hand)

Not automated on purpose. With a board plugged in:

```bash
cd firmware/gateway && ../../.venv/bin/pio run -e esp32dev -t upload
cd firmware/bar && ../../.venv/bin/pio run -e esp32dev -t upload
```

A freshly flashed bar has no ID and breathes dim white. With the bridge stopped (only one program can hold the gateway link), list the bars and give each an ID:

```bash
.venv/bin/stinger-config --out serial:/dev/cu.usbserial-XXXX list
.venv/bin/stinger-config --out serial:/dev/cu.usbserial-XXXX assign --mac 24:6f:28:aa:bb:cc --id 3
```

`stinger-patterns id-flash` then confirms each bar's position. `stinger-config` also sets `brightness` (cap, 0–255, never above the firmware limit) and `failsafe` (timeout in ms), per bar or for `all`.

### Things only real hardware can confirm

- ESP-NOW range and loss at 6 Mbps (D27), with 10 bars at 40 fps.
- FastLED output timing while the radio is busy (WS2812B on GPIO16 through the 74HCT125).
- Whether 2,000,000 baud is stable on the CH340C: set `serial_baud` and `STINGER_SERIAL_BAUD` together. 921600 leaves about 4 ms of each 25 ms frame spare (PROTOCOL.md section 8).
- RSSI: reported as 0 on the pinned Arduino core 2.x (D26).
- The first real Wokwi run (D35).
