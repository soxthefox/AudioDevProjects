"""Per-bar fps and dropped frames, and frame latency, for the simulator view."""

from collections import deque
from dataclasses import dataclass, field

from stinger_proto import Message, Show

WINDOW_MS = 1000
# A frame_seq jump larger than this is a resync (bar replugged, bridge restarted), not loss.
MAX_COUNTED_GAP = 100
LATENCY_SAMPLES = 40


@dataclass
class BarMetrics:
    latches: deque[int] = field(default_factory=deque)
    dropped: int = 0
    last_frame_seq: int | None = None

    def fps(self, now_ms: int) -> int:
        while self.latches and now_ms - self.latches[0] >= WINDOW_MS:
            self.latches.popleft()
        return len(self.latches)


class SimMetrics:
    def __init__(self) -> None:
        self.bars: dict[bytes, BarMetrics] = {}
        self._frame_start_ms: int | None = None
        self._latencies: deque[int] = deque(maxlen=LATENCY_SAMPLES)
        self._last_show_ms: int | None = None

    def bytes_arrived(self, now_ms: int) -> None:
        """Serial bytes reached the gateway; the first after a SHOW starts a frame."""
        if self._frame_start_ms is None:
            self._frame_start_ms = now_ms

    def host_message(self, message: Message, now_ms: int) -> None:
        if isinstance(message, Show) and self._frame_start_ms is not None:
            self._latencies.append(now_ms - self._frame_start_ms)
            self._frame_start_ms = None
            self._last_show_ms = now_ms

    def latched(self, mac: bytes, frame_seq: int, new_frame: bool, now_ms: int) -> None:
        bar = self.bars.setdefault(mac, BarMetrics())
        if bar.last_frame_seq is not None:
            gap = (frame_seq - bar.last_frame_seq) & 0xFFFF
            if 1 < gap <= MAX_COUNTED_GAP:
                bar.dropped += gap - 1  # whole SHOWs missed
        bar.last_frame_seq = frame_seq
        if new_frame:
            bar.latches.append(now_ms)
        else:
            bar.dropped += 1  # SHOW arrived but this bar's FRAME did not

    def latency_ms(self, now_ms: int) -> float | None:
        """Mean serial-in to SHOW time over recent frames; None once frames stop."""
        if self._last_show_ms is None or now_ms - self._last_show_ms >= WINDOW_MS:
            return None
        return sum(self._latencies) / len(self._latencies)

    def reset_bar(self, mac: bytes) -> None:
        self.bars.pop(mac, None)
