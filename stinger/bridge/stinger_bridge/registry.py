"""Bars the bridge has heard from (HELLO), with online/offline status."""

from dataclasses import dataclass

from stinger_proto import Hello


@dataclass
class BarEntry:
    mac: bytes
    bar_id: int
    led_count: int
    fw_version: int
    rssi: int
    last_seen_ms: int


@dataclass(frozen=True)
class BarStatus:
    entry: BarEntry
    online: bool
    silent_ms: int


class BarRegistry:
    def __init__(self, offline_after_ms: int) -> None:
        self._offline_after_ms = offline_after_ms
        self._by_mac: dict[bytes, BarEntry] = {}

    def hello(self, hello: Hello, now_ms: int) -> None:
        self._by_mac[hello.mac] = BarEntry(
            hello.mac, hello.bar_id, hello.led_count, hello.fw_version, hello.rssi, now_ms
        )

    def bars(self, now_ms: int) -> list[BarStatus]:
        """Every bar ever seen, sorted by ID (unassigned last), then MAC."""
        out = []
        for entry in self._by_mac.values():
            silent = now_ms - entry.last_seen_ms
            out.append(BarStatus(entry, silent < self._offline_after_ms, silent))
        return sorted(out, key=lambda s: (s.entry.bar_id == 0, s.entry.bar_id, s.entry.mac))

    def online_ids(self, now_ms: int) -> set[int]:
        return {s.entry.bar_id for s in self.bars(now_ms) if s.online and s.entry.bar_id}
