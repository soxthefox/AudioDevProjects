# LedFx setup

LedFx makes audio-reactive effects. It sends pixels as Art-Net or sACN (E1.31) to `stinger-bridge`. The simplest setup is one LedFx device of 600 pixels whose universe size is exactly one bar (180 channels). LedFx then fills universe 0 with bar 1, universe 1 with bar 2, and so on, which is the bridge's default mapping.

Field names below are from LedFx's device code (`ledfx/devices/artnet.py`, `e131.py`); the labels in the web UI may read slightly differently.

## 1. Start the receiving end first

```bash
make sim
.venv/bin/stinger-bridge --out sim:localhost:7777
```

(or `--out serial:/dev/cu.usbserial-XXXX` for the real gateway).

## 2. Add the device

In LedFx: **Devices → Add device**.

**Art-Net (recommended):**

| Field | Value | Why |
| --- | --- | --- |
| Type | Art-Net | |
| Name | Stinger | |
| IP address | `127.0.0.1` | the bridge on this machine |
| Pixel count | `600` | 10 bars × 60 |
| Universe | `0` | bar 1 is Art-Net universe 0 |
| Packet size (size of each DMX universe) | `180` | one bar per universe |
| DMX start address | `1` | |
| Port | `6454` | |
| Refresh rate | `40` | the bridge's frame rate |

**sACN / E1.31 (alternative):** type E1.31, IP `127.0.0.1`, pixel count `600`, universe `1` (sACN starts at 1; bar 1 is sACN universe 1), universe size `180`, channel offset `0`, refresh rate `40`. Use unicast to 127.0.0.1; multicast only reaches the bridge when it runs with `--listen 0.0.0.0`.

## 3. Check it

1. Run `.venv/bin/stinger-patterns rainbow` first, without LedFx. The bars should show a rainbow; this proves the bridge and simulator.
2. Stop the pattern tool, then in LedFx apply a **Gradient** effect to the Stinger device. In the simulator view, the colours should run from Bar 1 through Bar 10 in order, with no bar shifted or repeated. A shifted or repeated bar means the universe size is not 180.
3. If you need a different layout (bars in another order, or a gap), keep LedFx as is and change `config/bridge.yaml`: each bar can take any `artnet_universe`, `sacn_universe` and `start_channel`.

LedFx can also send DDP (UDP 4048). The bridge does not read DDP yet (HANDOVER.md lists it as optional).
