"""stinger-config: list bars, assign IDs, set brightness cap or failsafe timeout.

Talks to the gateway (or simulator) directly. The gateway link has room for one program: a serial
port can only be opened once, and the simulator hands its gateway to the newest connection. Stop
the bridge first, and start it again afterwards.
"""

import argparse
import asyncio
import contextlib
import sys

from stinger_bridge.link import GatewayLink, parse_out
from stinger_bridge.registry import BarRegistry
from stinger_config import load_installation
from stinger_proto import BAR_ALL, Config, ConfigKey, ConfigMac, Hello, Message

LISTEN_S = 1.5  # a little more than one HELLO interval


def parse_mac(text: str) -> bytes:
    mac = bytes.fromhex(text.replace(":", "").replace("-", ""))
    if len(mac) != 6:
        raise argparse.ArgumentTypeError("MAC is six hex bytes, e.g. 24:6f:28:aa:bb:cc")
    return mac


def parse_bar(text: str) -> int:
    return BAR_ALL if text == "all" else int(text)


async def listen_for_bars(link: GatewayLink, seconds: float, offline_after_ms: int) -> list[Hello]:
    registry = BarRegistry(offline_after_ms)

    async def collect() -> None:
        async for packet in link.packets():
            if isinstance(packet.message, Hello):
                registry.hello(packet.message, 0)

    with contextlib.suppress(TimeoutError):
        await asyncio.wait_for(collect(), seconds)
    hellos = []
    for status in registry.bars(0):
        e = status.entry
        hellos.append(Hello(e.bar_id, e.mac, e.led_count, e.fw_version, e.rssi))
    return hellos


async def run(args: argparse.Namespace) -> int:
    installation = load_installation()
    link = parse_out(args.out, args.baud or installation.serial_baud)
    await link.open()
    try:
        if args.command == "list":
            bars = await listen_for_bars(link, LISTEN_S, installation.offline_after_ms)
            for h in bars:
                print(f"{h.bar_id or '--':>3}  {h.mac.hex(':')}  {h.led_count} LEDs  rssi {h.rssi}")
            if not bars:
                print("no bars heard")
            return 0
        message: Message
        if args.command == "assign":
            message = ConfigMac(args.mac, ConfigKey.ASSIGN_ID, args.id)
        elif args.command == "brightness":
            message = Config(args.bar, ConfigKey.BRIGHTNESS_CAP, args.value)
        else:
            message = Config(args.bar, ConfigKey.FAILSAFE_TIMEOUT_MS, args.ms)
        link.send(message)
        await link.drain()
        print(f"sent {message}")
        return 0
    finally:
        await link.close()


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="stinger-config", description=__doc__)
    p.add_argument("--out", required=True, help="serial:DEVICE or sim:HOST:PORT")
    p.add_argument("--baud", type=int, help="serial baud (default: config/installation.yaml)")
    sub = p.add_subparsers(dest="command", required=True)
    sub.add_parser("list", help="show the bars that announce themselves")
    assign = sub.add_parser("assign", help="give the bar with this MAC an ID (0 unassigns)")
    assign.add_argument("--mac", type=parse_mac, required=True)
    assign.add_argument("--id", type=int, required=True, choices=range(0, 17), metavar="0-16")
    bright = sub.add_parser("brightness", help="lower the brightness cap (0-255)")
    bright.add_argument("--bar", type=parse_bar, default=BAR_ALL, help="1-16 or all")
    bright.add_argument("--value", type=int, required=True, choices=range(256), metavar="0-255")
    fail = sub.add_parser("failsafe", help="set the failsafe timeout in ms")
    fail.add_argument("--bar", type=parse_bar, default=BAR_ALL, help="1-16 or all")
    fail.add_argument("--ms", type=int, required=True)
    args = p.parse_args(argv)
    try:
        return asyncio.run(run(args))
    except (OSError, ValueError) as err:
        print(f"stinger-config: {err}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
