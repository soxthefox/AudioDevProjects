"""stinger-sim: start the simulator and print the URL to open."""

import argparse
import asyncio
import socket
import sys
from pathlib import Path

from stinger_config import Installation, load_installation

from stinger_sim.simulator import Simulator, now_ms
from stinger_sim.web import WebServer

PAGE = Path(__file__).resolve().parent / "static" / "index.html"
DEFAULT_HTTP_PORT = 8765
DEFAULT_GATEWAY_PORT = 7777
VIEW_FPS = 30


def parse_args(argv: list[str] | None, installation: Installation) -> argparse.Namespace:
    p = argparse.ArgumentParser(prog="stinger-sim", description=__doc__)
    p.add_argument(
        "--host", default="127.0.0.1", help="interface to listen on (0.0.0.0 shares on the LAN)"
    )
    p.add_argument("--http-port", type=int, default=DEFAULT_HTTP_PORT, help="browser view port")
    p.add_argument(
        "--gateway-port", type=int, default=DEFAULT_GATEWAY_PORT, help="virtual gateway TCP port"
    )
    p.add_argument("--bars", type=int, default=installation.bars, help="number of virtual bars")
    p.add_argument(
        "--baud",
        type=int,
        default=installation.serial_baud,
        help="simulated serial line speed; 0 for unlimited",
    )
    p.add_argument("--seed", type=int, default=0, help="random seed for packet loss")
    return p.parse_args(argv)


def lan_address() -> str:
    """This machine's LAN address (no packet is sent)."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        try:
            s.connect(("10.255.255.255", 1))
            return s.getsockname()[0]
        except OSError:
            return "127.0.0.1"


async def run(args: argparse.Namespace, installation: Installation) -> None:
    sim = Simulator(installation, args.bars, args.baud, args.seed)
    web = WebServer(PAGE, sim.command)
    gateway_server = await asyncio.start_server(sim.serve_serial, args.host, args.gateway_port)
    http_server = await asyncio.start_server(web.handle, args.host, args.http_port)

    shown_host = "localhost" if args.host in ("127.0.0.1", "localhost") else lan_address()
    print(f"Stinger simulator: {args.bars} bars x {installation.leds_per_bar} LEDs")
    print(f"  open      http://{shown_host}:{args.http_port}")
    gateway = f"sim:{shown_host}:{args.gateway_port}"
    print(f"  gateway   {gateway}   (bridge: --out {gateway})")
    print("  Ctrl-C to stop", flush=True)

    async def push_state() -> None:
        while True:
            web.broadcast(sim.snapshot(now_ms()))
            await asyncio.sleep(1 / VIEW_FPS)

    await asyncio.gather(
        sim.run_ticks(),
        push_state(),
        gateway_server.serve_forever(),
        http_server.serve_forever(),
    )


def main(argv: list[str] | None = None) -> int:
    installation = load_installation()
    args = parse_args(argv, installation)
    try:
        asyncio.run(run(args, installation))
    except KeyboardInterrupt:
        pass
    except OSError as err:
        print(f"stinger-sim: {err}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
