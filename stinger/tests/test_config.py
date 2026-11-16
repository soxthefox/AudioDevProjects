"""Firmware build flags must agree with config/installation.yaml (DECISIONS.md D14)."""

import configparser
import json
import re
from dataclasses import fields
from pathlib import Path

import pytest
from stinger_config import Installation, load_installation
from stinger_proto import WireDecoder, decode_packet

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
    installation = load_installation()
    known = {f.name for f in fields(Installation)}
    flags = esp32_flags(project)
    missing = REQUIRED[project] - flags.keys()
    assert not missing, f"{project}: missing build flags for {sorted(missing)}"
    for key, value in flags.items():
        if key in known:
            assert int(value) == getattr(installation, key), f"{project}: {key}"


def test_wokwi_strip_matches_bar_build_flags() -> None:
    diagram = json.loads((FIRMWARE / "bar" / "wokwi" / "diagram.json").read_text())
    flags = esp32_flags("bar")
    strip = next(p for p in diagram["parts"] if p["type"] == "wokwi-led-strip")
    assert int(strip["attrs"]["pixels"]) == int(flags["leds_per_bar"])
    assert ["strip:DIN", f"esp:{flags['data_pin']}"] in [c[:2] for c in diagram["connections"]]


def test_wokwi_scenario_is_valid_wire_input() -> None:
    import importlib.util

    path = FIRMWARE / "bar" / "wokwi" / "run_wokwi.py"
    spec = importlib.util.spec_from_file_location("run_wokwi", path)
    assert spec and spec.loader
    run_wokwi = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(run_wokwi)
    yaml_text = run_wokwi.scenario_yaml()
    raw = re.search(r"write-serial: \[([^\]]*)\]", yaml_text).group(1)
    wire = bytes(int(b) for b in raw.split(","))
    packets = list(WireDecoder().feed(wire))
    assert [type(decode_packet(p).message).__name__ for p in packets] == [
        "ConfigMac",
        "Frame",
        "Show",
    ]
    assert "pixel0=178,0,0" in yaml_text
