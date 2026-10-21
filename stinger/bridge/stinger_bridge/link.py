"""The laptop end of the gateway link: a real USB serial port, or the simulator over TCP."""

import asyncio
import queue
import socket
import threading
from abc import ABC, abstractmethod
from collections.abc import AsyncIterator

from stinger_proto import (
    DecodeError,
    Message,
    Packet,
    WireDecoder,
    decode_packet,
    encode_packet,
    encode_wire,
)

READ_SIZE = 4096
SERIAL_READ_TIMEOUT_S = 0.1


class GatewayLink(ABC):
    """Sends messages to the gateway and yields the packets it sends back."""

    def __init__(self) -> None:
        self._seq = 0
        self._decoder = WireDecoder()

    def send(self, message: Message) -> None:
        self._seq = (self._seq + 1) & 0xFFFF
        self._write(encode_wire(encode_packet(self._seq, message)))

    async def packets(self) -> AsyncIterator[Packet]:
        while data := await self._read():
            for item in self._decoder.feed(data):
                if isinstance(item, DecodeError):
                    continue
                try:
                    yield decode_packet(item)
                except DecodeError:
                    continue

    @property
    def rx_errors(self) -> int:
        return self._decoder.errors

    @abstractmethod
    async def open(self) -> None: ...

    @abstractmethod
    async def drain(self) -> None:
        """Wait until written bytes have been handed to the OS."""

    @abstractmethod
    async def close(self) -> None: ...

    @abstractmethod
    def _write(self, data: bytes) -> None: ...

    @abstractmethod
    async def _read(self) -> bytes:
        """Next chunk of bytes; empty when the link has closed."""


class TcpLink(GatewayLink):
    """The simulator's virtual gateway (sim:HOST:PORT)."""

    def __init__(self, host: str, port: int) -> None:
        super().__init__()
        self.host, self.port = host, port
        self._reader: asyncio.StreamReader | None = None
        self._writer: asyncio.StreamWriter | None = None

    async def open(self) -> None:
        self._reader, self._writer = await asyncio.open_connection(self.host, self.port)
        # A serial port sends bytes at once; do not let Nagle hold back small packets.
        sock = self._writer.get_extra_info("socket")
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    async def drain(self) -> None:
        if self._writer:
            await self._writer.drain()

    async def close(self) -> None:
        if self._writer:
            self._writer.close()

    def _write(self, data: bytes) -> None:
        if self._writer and not self._writer.is_closing():
            self._writer.write(data)

    async def _read(self) -> bytes:
        if self._reader is None:
            return b""
        try:
            return await self._reader.read(READ_SIZE)
        except ConnectionError:
            return b""

    def __str__(self) -> str:
        return f"sim:{self.host}:{self.port}"


class SerialLink(GatewayLink):
    """The real gateway on a USB serial port (serial:DEVICE). Opened only when asked for."""

    def __init__(self, device: str, baud: int) -> None:
        super().__init__()
        self.device, self.baud = device, baud
        self._port = None
        self._outbox: queue.Queue[bytes | None] = queue.Queue()
        self._writer_thread: threading.Thread | None = None

    async def open(self) -> None:
        import serial  # pyserial; imported here so nothing else needs it

        self._port = serial.Serial(self.device, self.baud, timeout=SERIAL_READ_TIMEOUT_S)
        # Blocking writes run on their own thread so the event loop keeps receiving Art-Net.
        self._writer_thread = threading.Thread(target=self._write_loop, daemon=True)
        self._writer_thread.start()

    async def drain(self) -> None:
        pass  # the writer thread owns pacing; the OS serial buffer applies back-pressure

    async def close(self) -> None:
        self._outbox.put(None)
        if self._port:
            self._port.close()

    def _write(self, data: bytes) -> None:
        self._outbox.put(data)

    def _write_loop(self) -> None:
        while (data := self._outbox.get()) is not None:
            self._port.write(data)

    async def _read(self) -> bytes:
        while self._port and self._port.is_open:
            data = await asyncio.to_thread(self._port.read, READ_SIZE)
            if data:
                return data
        return b""

    def __str__(self) -> str:
        return f"serial:{self.device}@{self.baud}"


def parse_out(spec: str, baud: int) -> GatewayLink:
    """`serial:/dev/ttyUSB0` or `sim:HOST:PORT`."""
    kind, _, rest = spec.partition(":")
    if kind == "serial" and rest:
        return SerialLink(rest, baud)
    if kind == "sim":
        host, _, port = rest.rpartition(":")
        if host and port.isdigit():
            return TcpLink(host, int(port))
    raise ValueError(f"--out must be serial:DEVICE or sim:HOST:PORT, not {spec!r}")
