"""moshviz feature service: live guitar audio in, OSC features out.

Usage:
    python service.py --list-devices
    python service.py --device "Scarlett" --channel 0
    python service.py --file take.wav          # replay a recording in real time (silent)
    python service.py --file show.mov --play   # play a recording (audio or video file) and
                                               # analyse what is playing; TouchDesigner can
                                               # follow /moshviz/time to keep a video in sync

OSC sent to TouchDesigner (default 127.0.0.1:9000), one bundle per hop:
    /moshviz/row/00 ... /moshviz/row/NN   per-strip mosh amount, 0..1
    /moshviz/onset                         0..1
    /moshviz/polyphony                     0..1
    /moshviz/pitch                         Hz, 0 when unvoiced
    /moshviz/level                         dBFS
    /moshviz/seq                           hop counter (for latency checks)
    /moshviz/time                          seconds of audio analysed so far (with --play:
                                           the playback position, used as the video clock)

OSC received (default port 9001), for live tuning from TouchDesigner:
    /moshviz/param/<name> <float>          any field of mapping.MappingParams
    /moshviz/clip/start <seconds>          with --play: loop from here
    /moshviz/clip/end <seconds>            with --play: loop back at this point (0 = file end)
"""

from __future__ import annotations

import argparse
import logging
import queue
import threading
import time
from pathlib import Path
from typing import NamedTuple

import numpy as np
from pythonosc import dispatcher, osc_bundle_builder, osc_message_builder, osc_server, udp_client

from features import FeatureConfig, FeatureExtractor, Features
from mapping import MappingParams, row_amounts

log = logging.getLogger("moshviz")

ADDRESS_PREFIX = "/moshviz"
STATUS_INTERVAL_S = 2.0
"""How often the worker logs live levels, for tuning the gate and threshold."""
QUEUE_MAX_BLOCKS = 64
"""About 340 ms of audio at the default hop. If the worker falls this far behind, drop blocks."""


class Block(NamedTuple):
    """One hop of mono audio and the stream time (seconds) at its start."""

    samples: np.ndarray
    time_s: float


def build_bundle(features: Features, rows: np.ndarray, seq: int,
                 time_s: float = 0.0) -> osc_bundle_builder.OscBundle:
    """Pack one hop of features into an OSC bundle (sent immediately)."""
    bundle = osc_bundle_builder.OscBundleBuilder(osc_bundle_builder.IMMEDIATELY)

    def add(address: str, value: float | int) -> None:
        msg = osc_message_builder.OscMessageBuilder(address=f"{ADDRESS_PREFIX}/{address}")
        msg.add_arg(value)
        bundle.add_content(msg.build())

    for i, amount in enumerate(rows):
        add(f"row/{i:02d}", float(amount))
    add("onset", features.onset)
    add("polyphony", features.polyphony)
    add("pitch", features.pitch_hz)
    add("level", features.level_db)
    add("seq", seq)
    add("time", time_s)
    return bundle.build()


class ClipWindow:
    """Trim points for --play, set live over OSC. end_s <= start_s means 'to the end'."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self.start_s = 0.0
        self.end_s = 0.0

    def set(self, name: str, seconds: float) -> None:
        with self._lock:
            setattr(self, f"{name}_s", max(0.0, seconds))

    def bounds(self, total_frames: int, sample_rate: float, min_frames: int) -> tuple[int, int]:
        """(start, end) in frames, clamped to the file, at least min_frames apart."""
        with self._lock:
            start = min(int(self.start_s * sample_rate), max(0, total_frames - min_frames))
            end = int(self.end_s * sample_rate) if self.end_s > self.start_s else total_frames
        return start, min(max(end, start + min_frames), total_frames)


class ParamServer:
    """Receives /moshviz/param/<name> (MappingParams) and /moshviz/clip/<start|end> (trim)."""

    def __init__(self, params: MappingParams, host: str, port: int, clip: ClipWindow | None = None):
        self._params = params
        self._lock = threading.Lock()
        self.clip = clip or ClipWindow()
        disp = dispatcher.Dispatcher()
        disp.map(f"{ADDRESS_PREFIX}/param/*", self._on_param)
        disp.map(f"{ADDRESS_PREFIX}/clip/*", self._on_clip)
        self._server = osc_server.ThreadingOSCUDPServer((host, port), disp)
        threading.Thread(target=self._server.serve_forever, daemon=True).start()
        log.info("listening for parameters on %s:%d", host, port)

    def _on_param(self, address: str, *args: object) -> None:
        name = address.rsplit("/", 1)[-1]
        if len(args) != 1 or not isinstance(args[0], (int, float)):
            log.warning("ignoring %s %r: expected one number", address, args)
            return
        try:
            with self._lock:
                self._params.set(name, float(args[0]))
            log.info("param %s = %s", name, args[0])
        except KeyError:
            log.warning("unknown parameter %r (known: %s)", name, ", ".join(MappingParams.names()))

    def _on_clip(self, address: str, *args: object) -> None:
        # TouchDesigner's OSC Out CHOP sends every sample since its last cook,
        # so a message can carry several values; the last one is the newest.
        name = address.rsplit("/", 1)[-1]
        if name not in ("start", "end") or not args or not isinstance(args[-1], (int, float)):
            log.warning("ignoring %s %r", address, args)
            return
        seconds = float(args[-1])
        if getattr(self.clip, f"{name}_s") != seconds:
            self.clip.set(name, seconds)
            log.info("clip %s = %.2f s", name, seconds)

    def snapshot(self) -> MappingParams:
        with self._lock:
            return MappingParams(**vars(self._params))


def run_worker(blocks: "queue.Queue[Block | None]", config: FeatureConfig,
               params: ParamServer, client: udp_client.UDPClient) -> None:
    """Consume audio blocks, compute features, send OSC. Returns on a None block."""
    extractor = FeatureExtractor(config)
    hops_per_status = max(1, round(STATUS_INTERVAL_S / config.hop_s))
    seq = 0
    while (block := blocks.get()) is not None:
        features = extractor.process(block.samples)
        rows = row_amounts(features.bands, features.polyphony, features.onset,
                           params.snapshot(), level_db=features.level_db)
        client.send(build_bundle(features, rows, seq, block.time_s))
        if seq % hops_per_status == 0:
            log.info("level %6.1f dBFS  polyphony %.2f  onset %.2f  rows mean %.2f max %.2f",
                     features.level_db, features.polyphony, features.onset,
                     float(rows.mean()), float(rows.max()))
        seq += 1


def _enqueue(blocks: "queue.Queue[Block | None]", block: Block) -> None:
    try:
        blocks.put_nowait(block)
    except queue.Full:
        log.warning("feature worker is behind; dropping a block")


def stream_device(blocks: "queue.Queue[Block | None]", config: FeatureConfig,
                  device: str | int | None, channel: int) -> None:
    import sounddevice as sd  # imported here so --file works without PortAudio

    frames_seen = 0

    def callback(indata: np.ndarray, frames: int, _time: object, status: sd.CallbackFlags) -> None:
        nonlocal frames_seen
        if status:
            log.warning("audio status: %s", status)
        _enqueue(blocks, Block(indata[:, channel].astype(np.float64), frames_seen / config.sample_rate))
        frames_seen += frames

    with sd.InputStream(device=device, channels=channel + 1, samplerate=config.sample_rate,
                        blocksize=config.hop, dtype="float32", latency="low",
                        callback=callback) as stream:
        log.info("audio in: %s @ %.0f Hz, input latency %.1f ms", stream.device,
                 stream.samplerate, stream.latency * 1e3)
        while True:
            time.sleep(1.0)


def load_audio(path: Path, sample_rate: float) -> np.ndarray:
    """Read a sound or video file as float64 (frames, channels) at ``sample_rate``.

    Sound files go through soundfile. Anything soundfile cannot read (for
    example the AAC track of a .mov) is decoded and resampled with PyAV.
    """
    import soundfile as sf

    try:
        audio, sr = sf.read(path, dtype="float64", always_2d=True)
        if sr != sample_rate:
            raise SystemExit(f"{path} is {sr} Hz; rerun with --sample-rate {sr}")
        return audio
    except sf.LibsndfileError:
        pass

    import av

    with av.open(str(path)) as container:
        stream = container.streams.audio[0]
        resampler = av.AudioResampler(format="flt", layout="stereo", rate=int(sample_rate))
        chunks = []
        for frame in container.decode(stream):
            for out in resampler.resample(frame):
                chunks.append(out.to_ndarray().reshape(-1, 2))  # packed float: interleaved L, R
        for out in resampler.resample(None):  # flush
            chunks.append(out.to_ndarray().reshape(-1, 2))
    return np.concatenate(chunks).astype(np.float64)


def stream_file(blocks: "queue.Queue[Block | None]", config: FeatureConfig,
                path: Path, loop: bool) -> None:
    """Feed a file in at the real-time rate, as if it were live input (silent)."""
    mono = load_audio(path, config.sample_rate).mean(axis=1)
    log.info("replaying %s (%.1f s)%s", path, len(mono) / config.sample_rate,
             " in a loop" if loop else "")
    next_time = time.perf_counter()
    while True:
        for start in range(0, len(mono) - config.hop + 1, config.hop):
            blocks.put(Block(mono[start:start + config.hop], start / config.sample_rate))
            next_time += config.hop_s
            time.sleep(max(0.0, next_time - time.perf_counter()))
        if not loop:
            return


def play_file(blocks: "queue.Queue[Block | None]", config: FeatureConfig,
              path: Path, loop: bool, clip: ClipWindow) -> None:
    """Play a file through the default output and analyse exactly what is played.

    The output callback is the clock: each hop it hands to the sound card is
    also queued for analysis, stamped with its position in the file. Video
    that follows /moshviz/time therefore stays in sync with the sound, and
    trimming the audio (``clip``) trims the video with it.
    """
    import sounddevice as sd

    audio = load_audio(path, config.sample_rate).astype(np.float32)
    total = len(audio)
    position = 0
    finished = threading.Event()

    def callback(outdata: np.ndarray, frames: int, _time: object, status: sd.CallbackFlags) -> None:
        nonlocal position
        if status:
            log.warning("audio status: %s", status)
        start, end = clip.bounds(total, config.sample_rate, min_frames=frames)
        if position < start:
            position = start
        if position + frames > end:
            if not loop:
                outdata.fill(0.0)
                finished.set()
                raise sd.CallbackStop
            position = start
        chunk = audio[position:position + frames]
        outdata[:] = chunk
        _enqueue(blocks, Block(chunk.mean(axis=1).astype(np.float64), position / config.sample_rate))
        position += frames

    with sd.OutputStream(samplerate=config.sample_rate, channels=audio.shape[1], blocksize=config.hop,
                         dtype="float32", latency="low", callback=callback) as stream:
        log.info("playing %s (%.1f s)%s, output latency %.1f ms", path, total / config.sample_rate,
                 " in a loop" if loop else "", stream.latency * 1e3)
        while not finished.wait(0.5):
            pass


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--list-devices", action="store_true", help="print audio devices and exit")
    parser.add_argument("--device", help="input device name substring or index")
    parser.add_argument("--channel", type=int, default=0, help="0-based input channel of the guitar")
    parser.add_argument("--file", type=Path, help="replay a sound file instead of live input")
    parser.add_argument("--loop", action="store_true", help="with --file: loop forever")
    parser.add_argument("--play", action="store_true",
                        help="with --file: play the audio and analyse what is playing (audio clock)")
    parser.add_argument("--sample-rate", type=float, default=FeatureConfig.sample_rate)
    parser.add_argument("--bands", type=int, default=FeatureConfig.n_bands, help="number of row strips")
    parser.add_argument("--set", action="append", default=[], metavar="NAME=VALUE",
                        help="start with a MappingParams value, e.g. --set threshold=0.7 (repeatable)")
    parser.add_argument("--osc-host", default="127.0.0.1")
    parser.add_argument("--osc-port", type=int, default=9000, help="TouchDesigner OSC In CHOP port")
    parser.add_argument("--param-port", type=int, default=9001, help="port for /moshviz/param/* messages")
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")

    if args.list_devices:
        import sounddevice as sd
        print(sd.query_devices())
        return

    config = FeatureConfig(sample_rate=args.sample_rate, n_bands=args.bands)
    mapping = MappingParams()
    for assignment in args.set:
        name, _, value = assignment.partition("=")
        try:
            mapping.set(name.strip(), float(value))
        except (KeyError, ValueError):
            parser.error(f"--set {assignment!r}: expected NAME=NUMBER with NAME in {MappingParams.names()}")
    params = ParamServer(mapping, "127.0.0.1", args.param_port)
    client = udp_client.UDPClient(args.osc_host, args.osc_port)
    log.info("sending OSC to %s:%d (%d strips)", args.osc_host, args.osc_port, config.n_bands)

    blocks: "queue.Queue[Block | None]" = queue.Queue(maxsize=QUEUE_MAX_BLOCKS)
    worker = threading.Thread(target=run_worker, args=(blocks, config, params, client), daemon=True)
    worker.start()
    try:
        if args.file and args.play:
            play_file(blocks, config, args.file, args.loop, params.clip)
        elif args.file:
            stream_file(blocks, config, args.file, args.loop)
        else:
            device = int(args.device) if args.device and args.device.isdigit() else args.device
            stream_device(blocks, config, device, args.channel)
    except KeyboardInterrupt:
        pass
    finally:
        blocks.put(None)
        worker.join(timeout=1.0)


if __name__ == "__main__":
    main()
