"""Installation facts shared by the bridge, simulator and tools (config/installation.yaml)."""

from dataclasses import dataclass, fields
from pathlib import Path

import yaml

DEFAULT_PATH = Path(__file__).resolve().parents[1] / "installation.yaml"


@dataclass(frozen=True)
class Installation:
    bars: int
    leds_per_bar: int
    target_fps: int
    serial_baud: int
    brightness_hard_limit: int
    failsafe_timeout_ms: int
    failsafe_fade_ms: int
    hello_interval_ms: int
    offline_after_ms: int
    stats_interval_ms: int
    config_retries: int
    input_hold_ms: int

    @property
    def frame_interval_ms(self) -> float:
        return 1000 / self.target_fps


def load_installation(path: Path = DEFAULT_PATH) -> Installation:
    raw = yaml.safe_load(path.read_text())
    expected = {f.name for f in fields(Installation)}
    if set(raw) != expected:
        missing, extra = expected - set(raw), set(raw) - expected
        raise ValueError(f"{path}: missing {sorted(missing)}, unknown {sorted(extra)}")
    return Installation(**{name: int(value) for name, value in raw.items()})
