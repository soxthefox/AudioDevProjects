"""A virtual bar. Mirrors firmware/bar/lib/bar_logic exactly (PROTOCOL.md section 6).

All arithmetic is integer so the simulator and the firmware produce identical pixels.
"""

from collections.abc import Callable
from dataclasses import dataclass

from stinger_proto import (
    BAR_ALL,
    BAR_UNASSIGNED,
    BROADCAST_MAC,
    MAX_BAR_ID,
    Config,
    ConfigKey,
    ConfigMac,
    DecodeError,
    Frame,
    Hello,
    Show,
    decode_packet,
)

FULL_SCALE = 255
MIN_FAILSAFE_TIMEOUT_MS = 100
# Unassigned bars breathe dim white: a triangle wave from 0 to this level and back.
BREATH_PEAK = 24
BREATH_PERIOD_MS = 4000


@dataclass(frozen=True)
class BarSettings:
    led_count: int
    hard_limit: int
    failsafe_timeout_ms: int
    failsafe_fade_ms: int
    hello_interval_ms: int
    fw_version: int


class VirtualBar:
    def __init__(
        self,
        mac: bytes,
        bar_id: int,
        settings: BarSettings,
        on_latch: Callable[["VirtualBar", int, bool, int], None] | None = None,
    ) -> None:
        self.mac = mac
        self.bar_id = bar_id
        self.settings = settings
        self.reboot()
        # Called on every SHOW with (bar, frame_seq, had_new_frame, now_ms); used for metrics.
        self._on_latch = on_latch

    def reboot(self) -> None:
        """Power-on state. The ID survives, as it does in NVS; everything else resets."""
        self.runtime_cap = self.settings.hard_limit
        self.failsafe_timeout_ms = self.settings.failsafe_timeout_ms
        self._pending = bytearray(3 * self.settings.led_count)
        self._shown = bytearray(3 * self.settings.led_count)
        self._new_frame = False
        self._last_frame_ms: int | None = None
        self._last_hello_ms: int | None = None

    @property
    def cap(self) -> int:
        return min(self.runtime_cap, self.settings.hard_limit)

    def receive(self, packet: bytes, now_ms: int) -> None:
        try:
            message = decode_packet(packet).message
        except DecodeError:
            return
        match message:
            case Frame():
                self._on_frame(message, now_ms)
            case Show():
                self._on_show(message, now_ms)
            case Config() if message.bar_id in (self.bar_id, BAR_ALL):
                self._apply_config(message.key, message.value)
            case ConfigMac() if message.mac in (self.mac, BROADCAST_MAC):
                self._apply_config(message.key, message.value)

    def poll_hello(self, now_ms: int) -> Hello | None:
        """Return a HELLO when one is due (every hello_interval_ms)."""
        due = self._last_hello_ms is None or (
            now_ms - self._last_hello_ms >= self.settings.hello_interval_ms
        )
        if not due:
            return None
        self._last_hello_ms = now_ms
        return Hello(self.bar_id, self.mac, self.settings.led_count, self.settings.fw_version, 0)

    def pixels(self, now_ms: int) -> bytes:
        """The R G B bytes the strip shows now, after cap, failsafe and breathing."""
        if self.bar_id == BAR_UNASSIGNED:
            level = self._breath_level(now_ms) * self.cap // FULL_SCALE
            return bytes([level]) * len(self._shown)
        fade = self._fade_level(now_ms)
        scale = self.cap * fade
        return bytes(v * scale // (FULL_SCALE * FULL_SCALE) for v in self._shown)

    def _on_frame(self, frame: Frame, now_ms: int) -> None:
        if frame.bar_id != self.bar_id or self.bar_id == BAR_UNASSIGNED:
            return
        n = min(len(frame.rgb), len(self._pending))
        self._pending[:n] = frame.rgb[:n]
        self._pending[n:] = bytes(len(self._pending) - n)
        self._new_frame = True
        self._last_frame_ms = now_ms

    def _on_show(self, show: Show, now_ms: int) -> None:
        self._shown[:] = self._pending
        if self._on_latch:
            self._on_latch(self, show.frame_seq, self._new_frame, now_ms)
        self._new_frame = False

    def _apply_config(self, key: ConfigKey, value: int) -> None:
        match key:
            case ConfigKey.BRIGHTNESS_CAP:
                self.runtime_cap = min(value, FULL_SCALE)
            case ConfigKey.FAILSAFE_TIMEOUT_MS:
                self.failsafe_timeout_ms = max(value, MIN_FAILSAFE_TIMEOUT_MS)
            case ConfigKey.ASSIGN_ID if value <= MAX_BAR_ID:
                self.bar_id = value
                self._last_hello_ms = None  # announce the new ID at once

    def _fade_level(self, now_ms: int) -> int:
        """255 while frames arrive, then a linear ramp to 0 over the fade time."""
        if self._last_frame_ms is None:
            return 0
        late = now_ms - self._last_frame_ms - self.failsafe_timeout_ms
        if late <= 0:
            return FULL_SCALE
        fade_ms = self.settings.failsafe_fade_ms
        if late >= fade_ms:
            return 0
        return FULL_SCALE - FULL_SCALE * late // fade_ms

    @staticmethod
    def _breath_level(now_ms: int) -> int:
        half = BREATH_PERIOD_MS // 2
        phase = now_ms % BREATH_PERIOD_MS
        ramp = phase if phase < half else BREATH_PERIOD_MS - phase
        return BREATH_PEAK * ramp // half
