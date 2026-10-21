"""The running bridge: UDP inputs -> frames -> gateway link, plus the live bar list."""

import asyncio
import contextlib
import socket
import time
from collections import deque
from collections.abc import Callable
from dataclasses import dataclass

from stinger_config import Installation
from stinger_proto import Frame, Hello, Ping, Pong, Show, Stats

from stinger_bridge import artnet, sacn
from stinger_bridge.dmx import DmxData, Sync
from stinger_bridge.frame import FrameAssembler, Pacer
from stinger_bridge.link import GatewayLink
from stinger_bridge.mapping import ARTNET, SACN, Mapping
from stinger_bridge.registry import BarRegistry

COALESCE_MS = 5.0
PING_INTERVAL_S = 1.0
LATENCY_SAMPLES = 400
U32 = 0xFFFFFFFF


def monotonic_ms() -> float:
    return time.monotonic() * 1000


@dataclass(frozen=True)
class InputConfig:
    listen_host: str
    artnet_port: int | None  # None = off
    sacn_port: int | None


class LatencyStats:
    """Input-to-SHOW times of recent frames, in ms."""

    def __init__(self) -> None:
        self.samples: deque[float] = deque(maxlen=LATENCY_SAMPLES)

    def add(self, ms: float) -> None:
        self.samples.append(ms)

    def percentile(self, p: float) -> float | None:
        if not self.samples:
            return None
        ordered = sorted(self.samples)
        return ordered[min(len(ordered) - 1, int(p / 100 * len(ordered)))]


class _UdpInput(asyncio.DatagramProtocol):
    def __init__(self, on_datagram: Callable[[bytes], None]) -> None:
        self._on_datagram = on_datagram

    def datagram_received(self, data: bytes, addr: tuple) -> None:
        self._on_datagram(data)


class Bridge:
    def __init__(
        self,
        installation: Installation,
        mapping: Mapping,
        link: GatewayLink,
        inputs: InputConfig,
        clock_ms: Callable[[], float] = monotonic_ms,
    ) -> None:
        self.installation = installation
        self.mapping = mapping
        self.link = link
        self.inputs = inputs
        self._clock_ms = clock_ms
        self.assembler = FrameAssembler(mapping)
        self.pacer = Pacer(installation.frame_interval_ms, COALESCE_MS, installation.input_hold_ms)
        self.registry = BarRegistry(installation.offline_after_ms)
        self.latency = LatencyStats()
        self.gateway_stats: Stats | None = None
        self.rtt_ms: float | None = None
        self.frames_sent = 0
        self.input_packets = 0
        self.on_show: Callable[[int, float], None] | None = None  # (frame_seq, now_ms), for tools
        self._frame_seq = 0
        self._wake = asyncio.Event()
        self._transports: list[asyncio.DatagramTransport] = []
        self.bound_ports: dict[str, int] = {}  # actual ports, useful when asked for port 0

    async def run(self) -> None:
        """Run until the gateway link closes."""
        await self.link.open()
        await self._open_inputs()
        tasks = [asyncio.create_task(c) for c in (self._send_loop(), self._ping_loop())]
        try:
            await self._receive_loop()
        finally:
            for t in tasks:
                t.cancel()
            for transport in self._transports:
                transport.close()
            await self.link.close()

    # ---- input ----

    def on_datagram(self, protocol: str, packet: bytes) -> None:
        decoded = (artnet if protocol == ARTNET else sacn).decode(packet)
        if decoded is None:
            return
        now = self._clock_ms()
        if isinstance(decoded, DmxData):
            if not self.assembler.update(protocol, decoded):
                return
            self.input_packets += 1
            self.pacer.input(now, ready=self.assembler.complete)
        elif isinstance(decoded, Sync) and self.pacer.pending_since is not None:
            self.pacer.input(now, ready=True)
        self._wake.set()

    async def _open_inputs(self) -> None:
        loop = asyncio.get_running_loop()
        for protocol, port in ((ARTNET, self.inputs.artnet_port), (SACN, self.inputs.sacn_port)):
            if port is None:
                continue
            sock = _udp_socket(self.inputs.listen_host, port)
            self.bound_ports[protocol] = sock.getsockname()[1]
            if protocol == SACN:
                _join_sacn_groups(sock, self.inputs.listen_host, self.mapping.universes(SACN))
            transport, _ = await loop.create_datagram_endpoint(
                lambda p=protocol: _UdpInput(lambda data: self.on_datagram(p, data)), sock=sock
            )
            self._transports.append(transport)

    # ---- output ----

    async def _send_loop(self) -> None:
        while True:
            due = self.pacer.next_send()
            now = self._clock_ms()
            if due is None or due > now:
                timeout = None if due is None else (due - now) / 1000
                self._wake.clear()
                with contextlib.suppress(TimeoutError):
                    await asyncio.wait_for(self._wake.wait(), timeout)
                continue
            self._send_frame(now)
            await self.link.drain()

    def _send_frame(self, now: float) -> None:
        started = self.pacer.pending_since
        for bar_id, rgb in self.assembler.take().items():
            self.link.send(Frame(bar_id, rgb))
        self._frame_seq = (self._frame_seq + 1) & 0xFFFF
        self.link.send(Show(self._frame_seq))
        sent_at = self._clock_ms()
        if started is not None:
            self.latency.add(sent_at - started)
        self.pacer.sent(now)
        self.frames_sent += 1
        if self.on_show:
            self.on_show(self._frame_seq, sent_at)

    async def _ping_loop(self) -> None:
        while True:
            self.link.send(Ping(int(self._clock_ms()) & U32))
            await asyncio.sleep(PING_INTERVAL_S)

    async def _receive_loop(self) -> None:
        async for packet in self.link.packets():
            message = packet.message
            if isinstance(message, Hello):
                self.registry.hello(message, int(self._clock_ms()))
            elif isinstance(message, Stats):
                self.gateway_stats = message
            elif isinstance(message, Pong):
                self.rtt_ms = (int(self._clock_ms()) - message.t_ms) & U32


def _udp_socket(host: str, port: int) -> socket.socket:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if hasattr(socket, "SO_REUSEPORT"):
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    sock.bind((host, port))
    return sock


def _join_sacn_groups(sock: socket.socket, host: str, universes: set[int]) -> None:
    """Multicast only works when listening on all interfaces; unicast sACN always works."""
    if host not in ("0.0.0.0", ""):
        return
    for universe in universes:
        group = socket.inet_aton(sacn.multicast_group(universe)) + socket.inet_aton("0.0.0.0")
        with contextlib.suppress(OSError):
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, group)
