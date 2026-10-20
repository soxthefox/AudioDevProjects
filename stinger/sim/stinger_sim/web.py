"""Minimal HTTP + WebSocket server (RFC 6455, text frames only), standard library only."""

import asyncio
import base64
import hashlib
import json
import struct
from collections.abc import Callable
from pathlib import Path

_WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
_OP_TEXT, _OP_CLOSE, _OP_PING, _OP_PONG = 0x1, 0x8, 0x9, 0xA
_MAX_CLIENT_MESSAGE = 64 * 1024


class WebServer:
    """Serves one static page at / and pushes JSON state to every client on /ws."""

    def __init__(self, page: Path, on_command: Callable[[dict], None]) -> None:
        self._page = page
        self._on_command = on_command
        self._clients: set[asyncio.StreamWriter] = set()

    async def handle(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        try:
            request = await reader.readuntil(b"\r\n\r\n")
            path, headers = _parse_request(request)
            if path == "/ws" and headers.get("upgrade", "").lower() == "websocket":
                await self._serve_websocket(reader, writer, headers)
            elif path in ("/", "/index.html"):
                _respond(writer, "200 OK", "text/html; charset=utf-8", self._page.read_bytes())
            else:
                _respond(writer, "404 Not Found", "text/plain", b"not found")
            await writer.drain()
        except (asyncio.IncompleteReadError, ConnectionError, asyncio.LimitOverrunError):
            pass
        finally:
            self._clients.discard(writer)
            writer.close()

    def broadcast(self, state: dict) -> None:
        frame = _encode_frame(_OP_TEXT, json.dumps(state, separators=(",", ":")).encode())
        for writer in list(self._clients):
            # Skip slow clients rather than queue stale state.
            transport = writer.transport
            if transport.is_closing():
                self._clients.discard(writer)
            elif transport.get_write_buffer_size() < len(frame) * 4:
                writer.write(frame)

    async def _serve_websocket(
        self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter, headers: dict
    ) -> None:
        accept = base64.b64encode(
            hashlib.sha1((headers["sec-websocket-key"] + _WS_GUID).encode()).digest()
        ).decode()
        writer.write(
            (
                "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                f"Connection: Upgrade\r\nSec-WebSocket-Accept: {accept}\r\n\r\n"
            ).encode()
        )
        self._clients.add(writer)
        while True:
            opcode, payload = await _read_frame(reader)
            if opcode == _OP_CLOSE:
                writer.write(_encode_frame(_OP_CLOSE, b""))
                return
            if opcode == _OP_PING:
                writer.write(_encode_frame(_OP_PONG, payload))
            elif opcode == _OP_TEXT:
                try:
                    self._on_command(json.loads(payload))
                except (ValueError, KeyError, TypeError):
                    pass  # ignore malformed commands from the page


def _parse_request(request: bytes) -> tuple[str, dict[str, str]]:
    lines = request.decode("latin-1").split("\r\n")
    path = lines[0].split(" ")[1]
    headers = {}
    for line in lines[1:]:
        name, _, value = line.partition(":")
        if name:
            headers[name.strip().lower()] = value.strip()
    return path, headers


def _respond(writer: asyncio.StreamWriter, status: str, content_type: str, body: bytes) -> None:
    head = (
        f"HTTP/1.1 {status}\r\nContent-Type: {content_type}\r\n"
        f"Content-Length: {len(body)}\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n"
    )
    writer.write(head.encode() + body)


def _encode_frame(opcode: int, payload: bytes) -> bytes:
    head = bytes([0x80 | opcode])
    n = len(payload)
    if n < 126:
        head += bytes([n])
    elif n < 1 << 16:
        head += bytes([126]) + struct.pack(">H", n)
    else:
        head += bytes([127]) + struct.pack(">Q", n)
    return head + payload


async def _read_frame(reader: asyncio.StreamReader) -> tuple[int, bytes]:
    b0, b1 = await reader.readexactly(2)
    opcode, masked, n = b0 & 0x0F, b1 & 0x80, b1 & 0x7F
    if n == 126:
        (n,) = struct.unpack(">H", await reader.readexactly(2))
    elif n == 127:
        (n,) = struct.unpack(">Q", await reader.readexactly(8))
    if n > _MAX_CLIENT_MESSAGE:
        raise ConnectionError("websocket message too large")
    mask = await reader.readexactly(4) if masked else b"\x00\x00\x00\x00"
    data = await reader.readexactly(n)
    return opcode, bytes(b ^ mask[i % 4] for i, b in enumerate(data))
