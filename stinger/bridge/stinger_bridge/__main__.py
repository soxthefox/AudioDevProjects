"""stinger-bridge: Art-Net / sACN in, Stinger frames out to the gateway or the simulator."""

import argparse
import asyncio
import sys
from pathlib import Path

from stinger_config import load_installation

from stinger_bridge import artnet, sacn, status
from stinger_bridge.app import Bridge, InputConfig, monotonic_ms
from stinger_bridge.link import parse_out
from stinger_bridge.mapping import DEFAULT_PATH, load_mapping

STATUS_INTERVAL_S = 1.0
CLEAR_SCREEN = "\x1b[H\x1b[J"


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    p = argparse.ArgumentParser(prog="stinger-bridge", description=__doc__)
    p.add_argument("--out", required=True, help="serial:DEVICE (real gateway) or sim:HOST:PORT")
    p.add_argument("--baud", type=int, help="serial baud (default: config/installation.yaml)")
    p.add_argument("--mapping", default=str(DEFAULT_PATH), help="universe mapping YAML")
    p.add_argument("--listen", default="127.0.0.1", help="interface for Art-Net/sACN input")
    p.add_argument("--artnet-port", type=int, default=artnet.PORT)
    p.add_argument("--sacn-port", type=int, default=sacn.PORT)
    p.add_argument("--no-artnet", action="store_true", help="do not listen for Art-Net")
    p.add_argument("--no-sacn", action="store_true", help="do not listen for sACN")
    return p.parse_args(argv)


async def show_status(bridge: Bridge) -> None:
    tty = sys.stdout.isatty()
    last_frames, last_online = 0, None
    while True:
        await asyncio.sleep(STATUS_INTERVAL_S)
        now = int(monotonic_ms())
        fps = (bridge.frames_sent - last_frames) / STATUS_INTERVAL_S
        last_frames = bridge.frames_sent
        online = bridge.registry.online_ids(now)
        # On a terminal, redraw in place; in a log, print only when the bar list changes.
        if tty:
            print(CLEAR_SCREEN + status.render(bridge, now, fps), flush=True)
        elif online != last_online:
            print(status.render(bridge, now, fps) + "\n", flush=True)
        last_online = online


async def run(args: argparse.Namespace) -> None:
    installation = load_installation()
    mapping = load_mapping(installation.bars, installation.leds_per_bar, Path(args.mapping))
    link = parse_out(args.out, args.baud or installation.serial_baud)
    inputs = InputConfig(
        args.listen,
        None if args.no_artnet else args.artnet_port,
        None if args.no_sacn else args.sacn_port,
    )
    bridge = Bridge(installation, mapping, link, inputs)
    status_task = asyncio.create_task(show_status(bridge))
    try:
        await bridge.run()
    finally:
        status_task.cancel()
    print("stinger-bridge: gateway link closed", file=sys.stderr)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        asyncio.run(run(args))
    except KeyboardInterrupt:
        return 0
    except (OSError, ValueError) as err:
        print(f"stinger-bridge: {err}", file=sys.stderr)
        return 1
    return 1


if __name__ == "__main__":
    sys.exit(main())
