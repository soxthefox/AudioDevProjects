"""M3 acceptance: Art-Net and sACN -> bridge -> simulator, and bridge latency under 10 ms."""

import asyncio
import contextlib
import socket

import pytest
from stinger_bridge import artnet, sacn
from stinger_bridge.app import Bridge, InputConfig
from stinger_bridge.link import TcpLink
from stinger_bridge.mapping import load_mapping
from stinger_config import load_installation
from stinger_sim.simulator import Simulator, now_ms
from stinger_tools.latency import measure_artnet

LATENCY_BUDGET_MS = 10.0


async def run_into_sim(protocol: str, color: bytes) -> list[bytes]:
    """Send a QLC+-style full universe per bar for 0.5 s; return the bars' pixels."""
    installation = load_installation()
    sim = Simulator(installation, installation.bars, baud=installation.serial_baud, seed=1)
    server = await asyncio.start_server(sim.serve_serial, "127.0.0.1", 0)
    gateway_port = server.sockets[0].getsockname()[1]
    mapping = load_mapping(installation.bars, installation.leds_per_bar)
    inputs = InputConfig(
        "127.0.0.1", 0 if protocol == "artnet" else None, 0 if protocol == "sacn" else None
    )
    bridge = Bridge(installation, mapping, TcpLink("127.0.0.1", gateway_port), inputs)
    task = asyncio.create_task(bridge.run())
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        while protocol not in bridge.bound_ports:
            await asyncio.sleep(0.01)
        port = bridge.bound_ports[protocol]
        for frame in range(20):
            for s in mapping.sources:
                universe_data = (color * installation.leds_per_bar).ljust(512, b"\x00")
                if protocol == "artnet":
                    packet = artnet.encode_dmx(s.artnet_universe, universe_data, frame)
                else:
                    packet = sacn.encode_dmx(s.sacn_universe, universe_data, frame, bytes(16))
                sock.sendto(packet, ("127.0.0.1", port))
            await asyncio.sleep(installation.frame_interval_ms / 1000)
        await asyncio.sleep(0.1)
        return [bar.pixels(now_ms()) for bar in sim.bars]
    finally:
        sock.close()
        task.cancel()
        with contextlib.suppress(asyncio.CancelledError):
            await task
        server.close()


@pytest.mark.parametrize("protocol", ["artnet", "sacn"])
def test_input_reaches_every_simulated_bar(protocol: str) -> None:
    installation = load_installation()
    pixels = asyncio.run(run_into_sim(protocol, b"\xff\x80\x00"))
    cap = installation.brightness_hard_limit
    expected = bytes([255 * cap // 255, 128 * cap // 255, 0]) * installation.leds_per_bar
    assert pixels == [expected] * installation.bars


def test_bridge_adds_under_10_ms() -> None:
    samples = sorted(asyncio.run(measure_artnet(load_installation(), frames=100)))
    p99 = samples[int(0.99 * len(samples)) - 1]
    assert p99 < LATENCY_BUDGET_MS, f"p99 {p99:.2f} ms"
