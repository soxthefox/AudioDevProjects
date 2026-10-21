"""stinger-latency: measure gateway round trip (ping) or the bridge's Art-Net-to-SHOW delay.

ping:    PING/PONG round trips to a gateway or the simulator (--out, like the bridge's).
         Do not run it against a serial port the bridge already has open.
artnet:  runs the bridge in-process between a local Art-Net sender and a stand-in gateway,
         and times each frame from the first ArtDmx sent to its SHOW arriving at the gateway.
"""

import argparse
import asyncio
import contextlib
import socket
import statistics
import sys
import time

from stinger_bridge import artnet
from stinger_bridge.app import Bridge, InputConfig
from stinger_bridge.link import TcpLink, parse_out
from stinger_bridge.mapping import load_mapping
from stinger_config import Installation, load_installation
from stinger_proto import Ping, Pong, Show, WireDecoder, decode_packet

PING_TIMEOUT_S = 1.0
FRAME_TIMEOUT_S = 1.0


def summary(samples_ms: list[float]) -> str:
    ordered = sorted(samples_ms)
    p99 = ordered[min(len(ordered) - 1, int(0.99 * len(ordered)))]
    return (
        f"n={len(ordered)}  min {ordered[0]:.2f}  median {statistics.median(ordered):.2f}  "
        f"p99 {p99:.2f}  max {ordered[-1]:.2f} ms"
    )


async def measure_ping(out: str, baud: int, count: int) -> list[float]:
    link = parse_out(out, baud)
    await link.open()
    packets = link.packets()
    rtts = []
    try:
        for i in range(count):
            t_ms = i + 1
            sent = time.perf_counter()
            link.send(Ping(t_ms))
            await link.drain()
            async with asyncio.timeout(PING_TIMEOUT_S):
                async for packet in packets:
                    if isinstance(packet.message, Pong) and packet.message.t_ms == t_ms:
                        break
            rtts.append((time.perf_counter() - sent) * 1000)
    finally:
        await link.close()
    return rtts


async def measure_artnet(installation: Installation, frames: int) -> list[float]:
    """Art-Net-to-SHOW delay of an in-process bridge, in ms, one sample per frame."""
    show_seen = asyncio.Event()

    async def stand_in_gateway(reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        decoder = WireDecoder()
        while data := await reader.read(65536):
            for item in decoder.feed(data):
                if isinstance(item, bytes) and isinstance(decode_packet(item).message, Show):
                    show_seen.set()

    server = await asyncio.start_server(stand_in_gateway, "127.0.0.1", 0)
    port = server.sockets[0].getsockname()[1]
    mapping = load_mapping(installation.bars, installation.leds_per_bar)
    bridge = Bridge(
        installation, mapping, TcpLink("127.0.0.1", port), InputConfig("127.0.0.1", 0, None)
    )
    bridge_task = asyncio.create_task(bridge.run())
    sender = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    samples = []
    try:
        async with asyncio.timeout(FRAME_TIMEOUT_S):
            while "artnet" not in bridge.bound_ports:
                await asyncio.sleep(0.01)
        artnet_port = bridge.bound_ports["artnet"]
        interval_s = 1 / installation.target_fps
        for frame in range(frames):
            show_seen.clear()
            data = bytes([frame & 0xFF]) * mapping.channels_per_bar
            start = time.perf_counter()
            for source in mapping.sources:
                packet = artnet.encode_dmx(source.artnet_universe, data, frame)
                sender.sendto(packet, ("127.0.0.1", artnet_port))
            async with asyncio.timeout(FRAME_TIMEOUT_S):
                await show_seen.wait()
            samples.append((time.perf_counter() - start) * 1000)
            await asyncio.sleep(max(0.0, interval_s - (time.perf_counter() - start)))
    finally:
        sender.close()
        bridge_task.cancel()
        with contextlib.suppress(asyncio.CancelledError):
            await bridge_task
        server.close()
    return samples


def main(argv: list[str] | None = None) -> int:
    installation = load_installation()
    p = argparse.ArgumentParser(
        prog="stinger-latency", description=__doc__, formatter_class=argparse.RawTextHelpFormatter
    )
    sub = p.add_subparsers(dest="mode", required=True)
    ping = sub.add_parser("ping", help="gateway PING/PONG round trip")
    ping.add_argument("--out", required=True, help="serial:DEVICE or sim:HOST:PORT")
    ping.add_argument("--baud", type=int, default=installation.serial_baud)
    ping.add_argument("--count", type=int, default=20)
    art = sub.add_parser("artnet", help="bridge Art-Net-to-SHOW delay")
    art.add_argument("--frames", type=int, default=200)
    args = p.parse_args(argv)
    try:
        if args.mode == "ping":
            samples = asyncio.run(measure_ping(args.out, args.baud, args.count))
            print("PING round trip:", summary(samples))
        else:
            samples = asyncio.run(measure_artnet(installation, args.frames))
            print("Art-Net in -> SHOW out:", summary(samples))
    except TimeoutError:
        print("stinger-latency: no answer (is the gateway or simulator running?)", file=sys.stderr)
        return 1
    except OSError as err:
        print(f"stinger-latency: {err}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
