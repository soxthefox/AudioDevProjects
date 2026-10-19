"""Convert a video to intra-frame ProRes so TouchDesigner can seek it freely.

Usage:
    python tools/intra_frame.py recordings/IMG_1422_2.MOV
    # writes recordings/IMG_1422_2_prores.mov (video only, 1280 x 720)

Why: in movie mode the Movie File In TOP is driven to a new time every
frame ("Specify Index", following the audio clock). Phone footage is
HEVC/H.264, which stores occasional full frames and predicts the rest, so
every seek has to decode forward from the last full frame, and playback
stalls. ProRes stores every frame whole, so any frame decodes on its own.
(The irony: inter-frame prediction is what the datamosh imitates.)

Output is 1280 x 720 because the Non-Commercial TouchDesigner licence caps
TOPs at 1280 anyway. Timestamps are copied from the source, so a variable
frame rate phone clip keeps its timing. The audio stays in the original
file, which the service plays.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import av

PRORES_PROXY_PROFILE = "0"   # prores_ks profiles: 0 proxy, 1 LT, 2 standard, 3 HQ
PRORES_PIXEL_FORMAT = "yuv422p10le"
DEFAULT_SIZE = (1280, 720)


def convert(source: Path, target: Path, size: tuple[int, int] = DEFAULT_SIZE) -> int:
    """Write an intra-frame ProRes copy of `source`'s video. Returns the frame count."""
    frames = 0
    with av.open(str(source)) as src, av.open(str(target), "w") as dst:
        video_in = src.streams.video[0]
        video_out = dst.add_stream("prores_ks", rate=video_in.average_rate)
        video_out.width, video_out.height = size
        video_out.pix_fmt = PRORES_PIXEL_FORMAT
        # Use the source clock (1/600 s on iPhone) for both the stream and the
        # encoder: the default encoder clock is 1/frame-rate, which rounds a
        # variable frame rate's shorter gaps onto the same tick and fails.
        video_out.time_base = video_in.time_base
        video_out.codec_context.time_base = video_in.time_base
        video_out.options = {"profile": PRORES_PROXY_PROFILE}
        for frame in src.decode(video_in):
            out = frame.reformat(width=size[0], height=size[1], format=PRORES_PIXEL_FORMAT)
            out.pts, out.time_base = frame.pts, frame.time_base
            for packet in video_out.encode(out):
                dst.mux(packet)
            frames += 1
        for packet in video_out.encode(None):  # flush
            dst.mux(packet)
    return frames


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", type=Path)
    parser.add_argument("--out", type=Path, help="default: <source>_prores.mov next to the source")
    args = parser.parse_args()
    target = args.out or args.source.with_name(args.source.stem + "_prores.mov")
    count = convert(args.source, target)
    print(f"wrote {target} ({count} frames, {target.stat().st_size / 1e6:.1f} MB)")


if __name__ == "__main__":
    main()
