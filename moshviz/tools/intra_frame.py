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
Phone clips have a variable frame rate, and their nominal rate can be far
from the real one (IMG_1423.MOV says 120 fps while averaging 29.5), so a
player that trusts the nominal rate plays them at the wrong speed (it did,
in TouchDesigner's free-running mode). The output is constant frame rate
instead (default 30 fps): output frame n shows the source frame on screen
at n / fps seconds, duplicating or dropping frames as needed, so real-time
timing is kept. The audio stays in the original file, which the service
plays.
"""

from __future__ import annotations

import argparse
from fractions import Fraction
from pathlib import Path

import av
import numpy as np

PRORES_PROXY_PROFILE = "0"   # prores_ks profiles: 0 proxy, 1 LT, 2 standard, 3 HQ
PRORES_PIXEL_FORMAT = "yuv422p10le"
MAX_SIDE = 1280              # Non-Commercial TouchDesigner TOP limit
DEFAULT_FPS = 30


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


def source_frames_for(source_times: list[float], fps: float, duration: float) -> list[int]:
    """For each output frame n (time n / fps), the index of the source frame on screen then.

    `source_times` are the source frames' presentation times, ascending.
    """
    picks, current = [], 0
    for n in range(int(duration * fps)):
        t = n / fps
        while current + 1 < len(source_times) and source_times[current + 1] <= t:
            current += 1
        picks.append(current)
    return picks


def convert(source: Path, target: Path, max_side: int = MAX_SIDE,
            fps: int = DEFAULT_FPS) -> tuple[int, tuple[int, int]]:
    """Write an upright, constant-frame-rate, intra-frame ProRes copy of `source`'s video.

    Returns (output frame count, output size).
    """
    with av.open(str(source)) as src:
        video_in = src.streams.video[0]
        times = [float(p.pts * video_in.time_base) for p in src.demux(video_in) if p.pts is not None]
    times.sort()
    start = times[0]
    times = [t - start for t in times]
    duration = times[-1] + 1.0 / fps
    picks = source_frames_for(times, fps, duration)

    written = 0
    with av.open(str(source)) as src, av.open(str(target), "w") as dst:
        video_in = src.streams.video[0]
        decoded = src.decode(video_in)
        first = next(decoded)
        turns = quarter_turns_of(getattr(first, "rotation", 0) or 0)
        size = output_size(first.width, first.height, turns, max_side)

        video_out = dst.add_stream("prores_ks", rate=Fraction(fps))
        video_out.width, video_out.height = size
        video_out.pix_fmt = PRORES_PIXEL_FORMAT
        video_out.time_base = Fraction(1, fps)
        video_out.codec_context.time_base = Fraction(1, fps)
        video_out.options = {"profile": PRORES_PROXY_PROFILE}

        def upright(frame: av.VideoFrame) -> av.VideoFrame:
            if turns:
                pixels = np.ascontiguousarray(np.rot90(frame.to_ndarray(format="rgb24"), turns))
                frame = av.VideoFrame.from_ndarray(pixels, format="rgb24")
            return frame.reformat(width=size[0], height=size[1], format=PRORES_PIXEL_FORMAT)

        # Decoded frames arrive in presentation order, matching `times`.
        index, current = 0, upright(first)
        for n, wanted in enumerate(picks):
            while index < wanted:
                current = upright(next(decoded))
                index += 1
            current.pts, current.time_base = n, Fraction(1, fps)
            for packet in video_out.encode(current):
                dst.mux(packet)
            written += 1
        for packet in video_out.encode(None):  # flush
            dst.mux(packet)
    return written, size


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", type=Path)
    parser.add_argument("--out", type=Path, help="default: <source>_prores.mov next to the source")
    parser.add_argument("--fps", type=int, default=DEFAULT_FPS, help="constant output frame rate")
    args = parser.parse_args()
    target = args.out or args.source.with_name(args.source.stem + "_prores.mov")
    count, size = convert(args.source, target, fps=args.fps)
    print(f"wrote {target} ({count} frames at {args.fps} fps = {count / args.fps:.2f} s, "
          f"{size[0]} x {size[1]}, {target.stat().st_size / 1e6:.1f} MB)")


if __name__ == "__main__":
    main()
