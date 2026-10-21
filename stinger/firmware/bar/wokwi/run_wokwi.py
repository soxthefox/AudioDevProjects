"""Run one simulated bar headless in Wokwi and check it lights, caps and fails safe.

Skips (exit 0) when WOKWI_CLI_TOKEN is not set. The token comes from the environment only.
The scenario feeds real wire-encoded packets into the bar's serial link:
  1. CONFIG_MAC to every bar: ASSIGN_ID 1
  2. FRAME for bar 1, all pixels full red, then SHOW
  3. expect pixel 0 = (hard limit, 0, 0), then black after the failsafe fade
"""

import configparser
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

from stinger_proto import (
    BROADCAST_MAC,
    ConfigKey,
    ConfigMac,
    Frame,
    Show,
    encode_packet,
    encode_wire,
)

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
PIO = PROJECT.parents[1] / ".venv" / "bin" / "pio"
SCENARIO = PROJECT / ".pio" / "wokwi" / "scenario.yaml"
TIMEOUT_MS = 30_000
BAR_ID = 1


def build_flags() -> dict[str, str]:
    ini = configparser.ConfigParser()
    ini.read(PROJECT / "platformio.ini")
    return dict(re.findall(r"-DSTINGER_(\w+)=(\S+)", ini["env:esp32dev"]["build_flags"]))


def scenario_yaml() -> str:
    flags = build_flags()
    leds, cap = int(flags["LED_COUNT"]), int(flags["BRIGHTNESS_HARD_LIMIT"])
    packets = [
        ConfigMac(BROADCAST_MAC, ConfigKey.ASSIGN_ID, BAR_ID),
        Frame(BAR_ID, b"\xff\x00\x00" * leds),
        Show(1),
    ]
    wire = b"".join(encode_wire(encode_packet(seq, p)) for seq, p in enumerate(packets, 1))
    return "\n".join(
        [
            "name: 'Stinger bar: assign, show, cap, failsafe'",
            "version: 1",
            "author: 'stinger'",
            "steps:",
            "  - delay: 1000ms",
            f"  - write-serial: [{', '.join(str(b) for b in wire)}]",
            f"  - wait-serial: 'pixel0={cap},0,0'",
            "  - wait-serial: 'pixel0=0,0,0'",
            "",
        ]
    )


def main() -> int:
    if not os.environ.get("WOKWI_CLI_TOKEN"):
        print("Wokwi: skipped (set WOKWI_CLI_TOKEN to run the single-bar simulation)")
        return 0
    cli = shutil.which("wokwi-cli")
    if cli is None:
        print("Wokwi: WOKWI_CLI_TOKEN is set but wokwi-cli is not on PATH.", file=sys.stderr)
        print("Install it from https://docs.wokwi.com/wokwi-ci/getting-started", file=sys.stderr)
        return 1
    subprocess.run([str(PIO), "run", "-e", "wokwi"], cwd=PROJECT, check=True)
    SCENARIO.parent.mkdir(parents=True, exist_ok=True)
    SCENARIO.write_text(scenario_yaml())
    result = subprocess.run(
        [cli, "--timeout", str(TIMEOUT_MS), "--scenario", str(SCENARIO), str(HERE)]
    )
    print("Wokwi: passed" if result.returncode == 0 else "Wokwi: FAILED")
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
