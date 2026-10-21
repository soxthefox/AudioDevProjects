"""stinger-config against the simulator: list bars, assign an ID, lower the brightness cap."""

import argparse
import asyncio
import contextlib

from stinger_config import load_installation
from stinger_sim.simulator import Simulator, sim_mac
from stinger_tools.configure import run


async def scenario(capsys) -> None:
    installation = load_installation()
    sim = Simulator(installation, 3, baud=0, seed=1)
    server = await asyncio.start_server(sim.serve_serial, "127.0.0.1", 0)
    port = server.sockets[0].getsockname()[1]
    ticks = asyncio.create_task(sim.run_ticks())
    out = f"sim:127.0.0.1:{port}"

    def args(**kw) -> argparse.Namespace:
        return argparse.Namespace(out=out, baud=0, **kw)

    try:
        assert await run(args(command="list")) == 0
        listed = capsys.readouterr().out
        assert sim_mac(2).hex(":") in listed

        assert await run(args(command="assign", mac=sim_mac(2), id=7)) == 0
        await asyncio.sleep(0.05)
        assert sim.bars[1].bar_id == 7

        # CONFIG by ID needs the gateway to have heard the bar; the sim announces every second.
        await asyncio.sleep(1.1)
        assert await run(args(command="brightness", bar=7, value=40)) == 0
        await asyncio.sleep(0.05)
        assert sim.bars[1].cap == 40
        assert sim.bars[0].cap == installation.brightness_hard_limit
    finally:
        ticks.cancel()
        with contextlib.suppress(asyncio.CancelledError):
            await ticks
        server.close()


def test_configure_tool_against_simulator(capsys) -> None:
    asyncio.run(scenario(capsys))
