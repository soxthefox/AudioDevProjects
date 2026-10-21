"""Assembling bar frames from DMX input, and deciding when to send them."""

from stinger_bridge.dmx import DmxData
from stinger_bridge.mapping import Mapping


class FrameAssembler:
    """The latest RGB for every bar, and which bars have new data since the last take()."""

    def __init__(self, mapping: Mapping) -> None:
        self._mapping = mapping
        self.rgb = {s.bar_id: bytes(mapping.channels_per_bar) for s in mapping.sources}
        self._updated: set[int] = set()

    def update(self, protocol: str, dmx: DmxData) -> bool:
        """Apply one universe. Returns True if it fed at least one bar."""
        n = self._mapping.channels_per_bar
        sources = self._mapping.bars_for(protocol, dmx.universe)
        for s in sources:
            start = s.start_channel - 1
            data = dmx.data[start : start + n]
            self.rgb[s.bar_id] = data + bytes(n - len(data))
            self._updated.add(s.bar_id)
        return bool(sources)

    @property
    def complete(self) -> bool:
        return self._updated == self.rgb.keys()

    def take(self) -> dict[int, bytes]:
        self._updated.clear()
        return dict(self.rgb)


class Pacer:
    """When to send the next frame. Times are in milliseconds, from any monotonic clock.

    A frame goes out as soon as every bar has new data or the sender signals sync, otherwise
    `coalesce_ms` after the first new data. A token bucket holds the average rate to one frame
    per `interval_ms` while letting a frame that arrives a little early go at once, so jitter in
    the input does not turn into added latency. When input stops, the last frame is repeated
    every two intervals for `hold_ms`, then sending stops.
    """

    BURST = 2  # frames that may go out back to back
    REPEAT_AFTER = 2  # intervals of silence before the last frame is repeated

    def __init__(self, interval_ms: float, coalesce_ms: float, hold_ms: float) -> None:
        self.interval_ms = interval_ms
        self.coalesce_ms = coalesce_ms
        self.hold_ms = hold_ms
        self.pending_since: float | None = None  # first input not yet sent
        self._ready = False
        self._last_input: float | None = None
        self._last_send: float | None = None
        self._tokens = float(self.BURST)
        self._tokens_at: float | None = None

    def input(self, now: float, ready: bool) -> None:
        if self.pending_since is None:
            self.pending_since = now
        self._last_input = now
        self._ready = self._ready or ready

    def next_send(self) -> float | None:
        """Time of the next send, or None if nothing should be sent."""
        if self.pending_since is not None:
            due = self.pending_since if self._ready else self.pending_since + self.coalesce_ms
            return max(due, self._token_available_at())
        if self._last_input is not None and self._last_send is not None:
            # Repeat only once input has paused for a while, so repeats never race live input.
            repeat = max(self._last_send, self._last_input) + self.REPEAT_AFTER * self.interval_ms
            if repeat - self._last_input <= self.hold_ms:
                return repeat
        return None

    def sent(self, now: float) -> None:
        self._tokens = self._tokens_now(now) - 1
        self._tokens_at = now
        self._last_send = now
        self.pending_since = None
        self._ready = False

    def _tokens_now(self, now: float) -> float:
        if self._tokens_at is None:
            return self._tokens
        return min(self.BURST, self._tokens + (now - self._tokens_at) / self.interval_ms)

    def _token_available_at(self) -> float:
        if self._tokens_at is None or self._tokens >= 1:
            return -float("inf")
        return self._tokens_at + (1 - self._tokens) * self.interval_ms
