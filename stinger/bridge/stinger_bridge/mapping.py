"""DMX universe/channel -> bar mapping, from config/bridge.yaml."""

from dataclasses import dataclass
from pathlib import Path

import yaml

from stinger_bridge.dmx import DMX_MAX

DEFAULT_PATH = Path(__file__).resolve().parents[2] / "config" / "bridge.yaml"
ARTNET, SACN = "artnet", "sacn"
_OVERRIDE_KEYS = {"artnet_universe", "sacn_universe", "start_channel"}


@dataclass(frozen=True)
class BarSource:
    bar_id: int
    artnet_universe: int
    sacn_universe: int
    start_channel: int  # 1-based


class Mapping:
    def __init__(self, sources: list[BarSource], leds_per_bar: int) -> None:
        self.sources = sources
        self.channels_per_bar = 3 * leds_per_bar
        for s in sources:
            if s.start_channel < 1 or s.start_channel - 1 + self.channels_per_bar > DMX_MAX:
                raise ValueError(f"bar {s.bar_id}: channels do not fit one universe")
        self._by_universe: dict[tuple[str, int], list[BarSource]] = {}
        for s in sources:
            self._by_universe.setdefault((ARTNET, s.artnet_universe), []).append(s)
            self._by_universe.setdefault((SACN, s.sacn_universe), []).append(s)

    def bars_for(self, protocol: str, universe: int) -> list[BarSource]:
        return self._by_universe.get((protocol, universe), [])

    def universes(self, protocol: str) -> set[int]:
        return {u for (p, u) in self._by_universe if p == protocol}


def load_mapping(bars: int, leds_per_bar: int, path: Path = DEFAULT_PATH) -> Mapping:
    raw = yaml.safe_load(path.read_text())
    overrides = raw.get("overrides") or {}
    sources = []
    for bar_id in range(1, bars + 1):
        o = overrides.get(bar_id, {})
        unknown = set(o) - _OVERRIDE_KEYS
        if unknown:
            raise ValueError(f"{path}: bar {bar_id}: unknown keys {sorted(unknown)}")
        sources.append(
            BarSource(
                bar_id=bar_id,
                artnet_universe=o.get("artnet_universe", raw["artnet_universe_base"] + bar_id - 1),
                sacn_universe=o.get("sacn_universe", raw["sacn_universe_base"] + bar_id - 1),
                start_channel=o.get("start_channel", raw["start_channel"]),
            )
        )
    return Mapping(sources, leds_per_bar)
