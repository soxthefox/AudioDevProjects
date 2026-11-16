"""The simulator's VirtualBar against the shared bar scenarios (also run on the firmware)."""

import json
from pathlib import Path

import pytest
from stinger_proto import encode_packet
from stinger_sim.bar import BarSettings, VirtualBar
from vectors import message_from_json

SCENARIOS = json.loads((Path(__file__).parent / "bar_scenarios.json").read_text())


@pytest.mark.parametrize("scenario", SCENARIOS["scenarios"], ids=lambda s: s["name"])
def test_bar_scenario(scenario: dict) -> None:
    bar = VirtualBar(
        bytes.fromhex(scenario["mac"].replace(":", "")),
        scenario["bar_id"],
        BarSettings(**SCENARIOS["settings"]),
    )
    for step in scenario["steps"]:
        at = step["at"]
        if "send" in step:
            message = message_from_json(step["send"])
            bar.receive(encode_packet(step["send"]["seq"], message), at)
        elif "expect" in step:
            assert bar.pixels(at).hex() == step["expect"], f"at {at} ms"
        else:
            hello = bar.poll_hello(at)
            got = None if hello is None else hello.bar_id
            assert got == step["expect_hello"], f"at {at} ms"


def test_reboot_keeps_id_and_resets_the_rest() -> None:
    settings = BarSettings(**SCENARIOS["settings"])
    bar = VirtualBar(b"\x02" * 6, 3, settings)
    bar.runtime_cap = 10
    bar.reboot()
    assert bar.bar_id == 3
    assert bar.cap == settings.hard_limit
    assert bar.pixels(0) == bytes(3 * settings.led_count)


def test_firmware_scenario_header_is_up_to_date() -> None:
    import gen_bar_scenarios

    expected = gen_bar_scenarios.render(SCENARIOS)
    assert gen_bar_scenarios.HEADER_PATH.read_text() == expected, "run tests/gen_bar_scenarios.py"
