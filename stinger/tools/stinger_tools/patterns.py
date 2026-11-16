"""stinger-patterns: send Art-Net (or sACN) test patterns to the bridge, without QLC+."""

import argparse
import colorsys
import socket
import sys
import time
import uuid
from collections.abc import Callable

from stinger_bridge import artnet, sacn
from stinger_bridge.mapping import DEFAULT_PATH, load_mapping
from stinger_config import load_installation

CHASE_LENGTH = 6
CHASE_SPEED_LEDS_PER_S = 60
RAINBOW_CYCLES_PER_S = 0.25
STROBE_HZ = 10
ID_FLASH_ON_S = 0.2
ID_FLASH_OFF_S = 0.2
ID_FLASH_PAUSE_S = 1.5

# A pattern gives one bar's R G B bytes: (bar_index from 0, bar_count, leds, t_seconds, color).
Pattern = Callable[[int, int, int, float, bytes], bytes]


def solid(bar: int, bars: int, leds: int, t: float, color: bytes) -> bytes:
    return color * leds


def chase(bar: int, bars: int, leds: int, t: float, color: bytes) -> bytes:
    """A block runs along bar 1, then bar 2, ... as if the bars were one long strip."""
    head = int(t * CHASE_SPEED_LEDS_PER_S) % (bars * leds) - bar * leds
    return b"".join(color if 0 <= head - i < CHASE_LENGTH else b"\0\0\0" for i in range(leds))


def rainbow(bar: int, bars: int, leds: int, t: float, color: bytes) -> bytes:
    out = bytearray()
    for i in range(leds):
        hue = (i / leds + bar / bars + t * RAINBOW_CYCLES_PER_S) % 1.0
        out += bytes(round(c * 255) for c in colorsys.hsv_to_rgb(hue, 1.0, 1.0))
    return bytes(out)


def strobe(bar: int, bars: int, leds: int, t: float, color: bytes) -> bytes:
    on = (t * STROBE_HZ) % 1.0 < 0.5
    return (color if on else b"\0\0\0") * leds


def id_flash(bar: int, bars: int, leds: int, t: float, color: bytes) -> bytes:
    """Bar N flashes N times, then all pause; count the flashes to identify a bar."""
    blink = ID_FLASH_ON_S + ID_FLASH_OFF_S
    cycle = bars * blink + ID_FLASH_PAUSE_S
    phase = t % cycle
    on = phase < (bar + 1) * blink and phase % blink < ID_FLASH_ON_S
    return (color if on else b"\0\0\0") * leds


PATTERNS: dict[str, Pattern] = {
    "solid": solid,
    "chase": chase,
    "rainbow": rainbow,
    "strobe": strobe,
    "id-flash": id_flash,
}


def parse_color(text: str) -> bytes:
    color = bytes.fromhex(text.removeprefix("#"))
    if len(color) != 3:
        raise argparse.ArgumentTypeError("color is RRGGBB hex")
    return color


def main(argv: list[str] | None = None) -> int:
    installation = load_installation()
    p = argparse.ArgumentParser(prog="stinger-patterns", description=__doc__)
    p.add_argument("pattern", choices=sorted(PATTERNS))
    p.add_argument("--color", type=parse_color, default=b"\xff\xff\xff", help="RRGGBB")
    p.add_argument("--host", default="127.0.0.1", help="where the bridge listens")
    p.add_argument("--sacn", action="store_true", help="send sACN (E1.31) instead of Art-Net")
    p.add_argument("--port", type=int, help="UDP port (default 6454, or 5568 with --sacn)")
    p.add_argument("--fps", type=float, default=installation.target_fps)
    p.add_argument("--seconds", type=float, default=0, help="stop after this long; 0 = forever")
    args = p.parse_args(argv)

    mapping = load_mapping(installation.bars, installation.leds_per_bar, DEFAULT_PATH)
    port = args.port or (sacn.PORT if args.sacn else artnet.PORT)
    pattern = PATTERNS[args.pattern]
    bars = len(mapping.sources)
    print(
        f"{args.pattern} -> {'sACN' if args.sacn else 'Art-Net'} {args.host}:{port}, Ctrl-C to stop"
    )

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    cid = uuid.uuid4().bytes  # one sACN source identity for the whole run
    start = time.monotonic()
    frame = 0
    try:
        while not args.seconds or time.monotonic() - start < args.seconds:
            t = time.monotonic() - start
            for i, source in enumerate(mapping.sources):
                rgb = pattern(i, bars, installation.leds_per_bar, t, args.color)
                data = bytes(source.start_channel - 1) + rgb
                if args.sacn:
                    packet = sacn.encode_dmx(source.sacn_universe, data, frame, cid)
                else:
                    packet = artnet.encode_dmx(source.artnet_universe, data, frame)
                sock.sendto(packet, (args.host, port))
            frame += 1
            time.sleep(max(0.0, start + frame / args.fps - time.monotonic()))
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
