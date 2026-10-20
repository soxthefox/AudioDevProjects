"""Simulated ESP-NOW: instant delivery to plugged-in bars, with optional random loss."""

import random
from collections.abc import Callable

from stinger_sim.bar import VirtualBar


class SimRadio:
    def __init__(
        self,
        bars: list[VirtualBar],
        clock_ms: Callable[[], int],
        rng: random.Random | None = None,
    ) -> None:
        self.bars = bars
        self.loss = 0.0  # probability that one bar misses one packet
        self.unplugged: set[bytes] = set()
        self._clock_ms = clock_ms
        self._rng = rng or random.Random()

    def is_plugged(self, bar: VirtualBar) -> bool:
        return bar.mac not in self.unplugged

    def broadcast(self, packet: bytes) -> bool:
        for bar in self.bars:
            self._deliver(bar, packet)
        return True

    def unicast(self, mac: bytes, packet: bytes) -> bool:
        bar = next((b for b in self.bars if b.mac == mac), None)
        return bar is not None and self._deliver(bar, packet)

    def drops_packet(self) -> bool:
        """One random draw against the configured loss rate."""
        return self._rng.random() < self.loss

    def _deliver(self, bar: VirtualBar, packet: bytes) -> bool:
        if not self.is_plugged(bar) or self.drops_packet():
            return False
        bar.receive(packet, self._clock_ms())
        return True
