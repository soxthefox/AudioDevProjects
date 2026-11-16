# QLC+ setup

Drives the 10 bars from QLC+ over Art-Net, through `stinger-bridge`. Every bar is one QLC+ universe of 60 RGB pixels (180 channels starting at address 1).

| Bar | QLC+ universe | Art-Net universe | Channels |
| --- | --- | --- | --- |
| 1 | 1 | 0 | 1–180 |
| 2 | 2 | 1 | 1–180 |
| … | … | … | … |
| 10 | 10 | 9 | 1–180 |

Menu names follow the QLC+ 4 documentation; check them against your version.

QLC+ numbers universes from 1 and, by default, sends QLC+ universe N as Art-Net universe N − 1, which matches the bridge's default mapping (`config/bridge.yaml`).

## 1. Start the receiving end first

Simulator (no hardware):

```bash
make sim
.venv/bin/stinger-bridge --out sim:localhost:7777
```

Real gateway (you plug it in; find the port with `ls /dev/cu.*`):

```bash
.venv/bin/stinger-bridge --out serial:/dev/cu.usbserial-XXXX
```

The bridge listens for Art-Net on 127.0.0.1:6454. If QLC+ also holds port 6454 (for Art-Net input), the bridge's bind to the specific loopback address should still receive unicast to 127.0.0.1 (DECISIONS.md D19). If the bridge reports "address already in use", start it before QLC+.

## 2. Universes and Art-Net output

1. **Inputs/Outputs** tab. Add universes until there are 10 (the `+` button under the universe list).
2. For each universe 1–10, tick **Output** on the **ArtNet** line whose interface is **127.0.0.1**. If the loopback line is missing, pick the interface you want and run the bridge with `--listen 0.0.0.0` instead.
3. Select the ArtNet output and open its configuration (the wrench button). For every universe:
   - **IP Address**: `127.0.0.1` (unicast to the bridge on this machine).
   - **Universe**: leave the default (QLC+ universe − 1).
   - **Transmission mode**: **Full**. QLC+ then sends all 512 channels of every universe at its 50 Hz clock. In Standard mode it only sends a universe when a channel changes, or every 2 seconds. The bars' failsafe blanks them after 2 s without a frame, so a static look in Standard mode can flicker out.

QLC+ runs at 50 Hz and the bridge sends at most 40 fps (the serial link's budget at 921600 baud). The bridge drops the surplus and always sends the newest frame.

## 3. Fixtures: 10 bars of 60 RGB pixels

**Recommended: one RGB panel per bar.** In the **Fixtures** tab, use **Add RGB Panel** ten times:

| Field | Value |
| --- | --- |
| Name | `Bar 1` … `Bar 10` |
| Universe | 1 … 10 (bar N on universe N) |
| Address | 1 |
| Components | RGB |
| Columns | 60 |
| Rows | 1 |
| Orientation | Top-Left (pixel 1 at the bar's data-in end) |

Then make one fixture group for effects: **Fixture Groups → New group**, size 60 × 10, and drag the ten bar fixtures in, one per row, Bar 1 at the top. RGB Matrix effects on that group play across all bars like one 60 × 10 screen.

**Alternative:** one RGB Panel with 60 columns × 10 rows. QLC+ creates one fixture per row, but packs them into as few universes as possible (two bars per 512-channel universe). Afterwards, open each row fixture's properties and move bar N to universe N, address 1, so each bar has its own universe.

## 4. Check it

1. `.venv/bin/stinger-patterns id-flash` without QLC+ first: bar N flashes N times. This proves the bridge, mapping and bars.
2. In QLC+, **Simple Desk**: universe 1, channel 1 to 255. Bar 1's first pixel goes red (at the 70 % hard limit).
3. An RGB Matrix "Plasma" on the group should flow across all ten bars in the simulator view.

The bridge console shows `input->SHOW` latency, frames per second and which bars are online.
