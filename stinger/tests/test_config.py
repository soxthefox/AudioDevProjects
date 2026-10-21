"""Firmware build flags must agree with config/installation.yaml (DECISIONS.md D14)."""

import configparser
import re
from dataclasses import fields
from pathlib import Path

import pytest
from stinger_config import Installation, load_installation

FIRMWARE = Path(__file__).resolve().parents[1] / "firmware"
FLAG = re.compile(r"-DSTINGER_(\w+)=(\S+)")
# Installation facts each firmware must take from its build flags.
REQUIRED = {
    "gateway": {"serial_baud", "stats_interval_ms", "config_retries", "offline_after_ms"},
    "bar": {
        "leds_per_bar",
        "brightness_hard_limit",
        "failsafe_timeout_ms",
        "failsafe_fade_ms",
        "hello_interval_ms",
    },
}
# Flag names that differ from the YAML key.
FLAG_TO_KEY = {"led_count": "leds_per_bar"}


def esp32_flags(project: str) -> dict[str, str]:
    ini = configparser.ConfigParser()
    ini.read(FIRMWARE / project / "platformio.ini")
    flags = ini["env:esp32dev"]["build_flags"]
    return {FLAG_TO_KEY.get(k.lower(), k.lower()): v for k, v in FLAG.findall(flags)}


@pytest.mark.parametrize("project", sorted(REQUIRED))
def test_firmware_flags_match_installation(project: str) -> None:
    if not (FIRMWARE / project / "platformio.ini").exists():
        pytest.skip(f"{project} firmware not written yet")
    installation = load_installation()
    known = {f.name for f in fields(Installation)}
    flags = esp32_flags(project)
    missing = REQUIRED[project] - flags.keys()
    assert not missing, f"{project}: missing build flags for {sorted(missing)}"
    for key, value in flags.items():
        if key in known:
            assert int(value) == getattr(installation, key), f"{project}: {key}"
