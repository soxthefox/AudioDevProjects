"""M2 acceptance: the bridge's bar list follows unplug/replug in the simulator within 3 s."""

import asyncio
import time

from stinger_bridge.link import TcpLink
from stinger_bridge.registry import BarRegistry
from stinger_config import load_installation
from stinger_proto import Hello, Stats
from stinger_sim.simulator import Simulator, now_ms

POLL_S = 0.05
UNPLUGGED_BAR = 3


async def wait_until(condition, timeout_s: float) -> float:
    """Seconds until condition() is true; raises if it never is."""
    start = time.monotonic()
    while not condition():
        if time.monotonic() - start > timeout_s:
            raise AssertionError(f"condition not met within {timeout_s} s")
        await asyncio.sleep(POLL_S)
    return time.monotonic() - start


async def scenario() -> None:
    installation = load_installation()
    sim = Simulator(installation, installation.bars, baud=0, seed=1)
    server = await asyncio.start_server(sim.serve_serial, "127.0.0.1", 0)
    port = server.sockets[0].getsockname()[1]
    ticks = asyncio.create_task(sim.run_ticks())

    link = TcpLink("127.0.0.1", port)
    await link.open()
    registry = BarRegistry(installation.offline_after_ms)
    stats: list[Stats] = []

    async def listen() -> None:
        async for packet in link.packets():
            if isinstance(packet.message, Hello):
                registry.hello(packet.message, now_ms())
            elif isinstance(packet.message, Stats):
                stats.append(packet.message)

    listener = asyncio.create_task(listen())
    try:
        every_bar = set(range(1, installation.bars + 1))
        await wait_until(lambda: registry.online_ids(now_ms()) == every_bar, 2.0)

        sim.set_plugged(UNPLUGGED_BAR, False)
        limit_s = installation.offline_after_ms / 1000
        took = await wait_until(
            lambda: UNPLUGGED_BAR not in registry.online_ids(now_ms()), limit_s + 0.5
        )
        assert took <= limit_s + 0.1, f"bar went offline after {took:.2f} s"
        assert registry.online_ids(now_ms()) == every_bar - {UNPLUGGED_BAR}

        sim.set_plugged(UNPLUGGED_BAR, True)
        await wait_until(lambda: UNPLUGGED_BAR in registry.online_ids(now_ms()), 0.5)
        assert stats, "gateway sends STATS every second"
    finally:
        listener.cancel()
        ticks.cancel()
        await link.close()
        server.close()


def test_unplug_and_replug_reach_the_bridge_list() -> None:
    asyncio.run(scenario())
