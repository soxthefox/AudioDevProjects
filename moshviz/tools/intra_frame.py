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

The longer side is scaled to 1280 (the Non-Commercial TouchDesigner
licence caps TOPs at 1280 anyway), keeping the aspect ratio. Phone clips
filmed upright are stored sideways with a rotation tag; the rotation is
applied here, so a vertical clip comes out as 720 x 1280 portrait.
Timestamps are copied from the source, so a variable frame rate phone clip
keeps its timing. The audio stays in the original file, which the service
plays.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import av
import numpy as np

PRORES_PROXY_PROFILE = "0"   # prores_ks profiles: 0 proxy, 1 LT, 2 standard, 3 HQ
PRORES_PIXEL_FORMAT = "yuv422p10le"
MAX_SIDE = 1280              # Non-Commercial TouchDesigner TOP limit


def output_size(width: int, height: int, quarter_turns: int, max_side: int = MAX_SIDE) -> tuple[int, int]:
    """Upright size after rotation, longer side scaled to max_side, both sides even."""
    if quarter_turns % 2:
        width, height = height, width
    scale = min(1.0, max_side / max(width, height))
    return (int(round(width * scale / 2)) * 2, int(round(height * scale / 2)) * 2)


def quarter_turns_of(rotation_degrees: float) -> int:
    """np.rot90 turns (counter-clockwise) that make a frame upright.

    PyAV reports the display rotation in degrees counter-clockwise; an
    upright iPhone clip reads -90, which needs one clockwise turn (-1).
    """
    return int(round(rotation_degrees / 90.0)) % 4


def convert(source: Path, target: Path, max_side: int = MAX_SIDE) -> tuple[int, tuple[int, int]]:
    """Write an upright, intra-frame ProRes copy of `source`'s video.

    Returns (frame count, output size).
    """
    frames = 0
    with av.open(str(source)) as src, av.open(str(target), "w") as dst:
        video_in = src.streams.video[0]
        first = next(src.decode(video_in))
        turns = quarter_turns_of(getattr(first, "rotation", 0) or 0)
        size = output_size(first.width, first.height, turns, max_side)
        src.seek(0)
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
            if turns:
                upright = np.ascontiguousarray(np.rot90(frame.to_ndarray(format="rgb24"), turns))
                frame_up = av.VideoFrame.from_ndarray(upright, format="rgb24")
            else:
                frame_up = frame
            out = frame_up.reformat(width=size[0], height=size[1], format=PRORES_PIXEL_FORMAT)
            out.pts, out.time_base = frame.pts, frame.time_base
            for packet in video_out.encode(out):
                dst.mux(packet)
            frames += 1
        for packet in video_out.encode(None):  # flush
            dst.mux(packet)
    return frames, size


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", type=Path)
    parser.add_argument("--out", type=Path, help="default: <source>_prores.mov next to the source")
    args = parser.parse_args()
    target = args.out or args.source.with_name(args.source.stem + "_prores.mov")
    count, size = convert(args.source, target)
    print(f"wrote {target} ({count} frames, {size[0]} x {size[1]}, {target.stat().st_size / 1e6:.1f} MB)")


if __name__ == "__main__":
    main()
