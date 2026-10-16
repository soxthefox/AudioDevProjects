"""moshviz feature service: live guitar audio in, OSC features out.

Usage:
    python service.py --list-devices
    python service.py --device "Scarlett" --channel 0
    python service.py --file take.wav          # replay a recording in real time

OSC sent to TouchDesigner (default 127.0.0.1:9000), one bundle per hop:
    /moshviz/row/00 ... /moshviz/row/NN   per-strip mosh amount, 0..1
    /moshviz/onset                         0..1
    /moshviz/polyphony                     0..1
    /moshviz/pitch                         Hz, 0 when unvoiced
    /moshviz/level                         dBFS
    /moshviz/seq                           hop counter (for latency checks)

OSC received (default port 9001), for live tuning from TouchDesigner:
    /moshviz/param/<name> <float>          any field of mapping.MappingParams
"""

from __future__ import annotations

import argparse
import logging
import queue
import threading
import time
from pathlib import Path

import numpy as np
from pythonosc import dispatcher, osc_bundle_builder, osc_message_builder, osc_server, udp_client

from features import FeatureConfig, FeatureExtractor, Features
from mapping import MappingParams, row_amounts

log = logging.getLogger("moshviz")

ADDRESS_PREFIX = "/moshviz"
QUEUE_MAX_BLOCKS = 64
"""About 340 ms of audio at the default hop. If the worker falls this far behind, drop blocks."""


def build_bundle(features: Features, rows: np.ndarray, seq: int) -> osc_bundle_builder.OscBundle:
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
    return bundle.build()


class ParamServer:
    """Receives /moshviz/param/<name> messages and updates MappingParams in place."""

    def __init__(self, params: MappingParams, host: str, port: int):
        self._params = params
        self._lock = threading.Lock()
        disp = dispatcher.Dispatcher()
        disp.map(f"{ADDRESS_PREFIX}/param/*", self._on_param)
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

    def snapshot(self) -> MappingParams:
        with self._lock:
            return MappingParams(**vars(self._params))


def run_worker(blocks: "queue.Queue[np.ndarray | None]", config: FeatureConfig,
               params: ParamServer, client: udp_client.UDPClient) -> None:
    """Consume audio blocks, compute features, send OSC. Returns on a None block."""
    extractor = FeatureExtractor(config)
    seq = 0
    while (block := blocks.get()) is not None:
        features = extractor.process(block)
        rows = row_amounts(features.bands, features.polyphony, features.onset, params.snapshot())
        client.send(build_bundle(features, rows, seq))
        seq += 1


def stream_device(blocks: "queue.Queue[np.ndarray | None]", config: FeatureConfig,
                  device: str | int | None, channel: int) -> None:
    import sounddevice as sd  # imported here so --file works without PortAudio

    def callback(indata: np.ndarray, frames: int, _time: object, status: sd.CallbackFlags) -> None:
        if status:
            log.warning("audio status: %s", status)
        try:
            blocks.put_nowait(indata[:, channel].astype(np.float64))
        except queue.Full:
            log.warning("feature worker is behind; dropping a block")

    with sd.InputStream(device=device, channels=channel + 1, samplerate=config.sample_rate,
                        blocksize=config.hop, dtype="float32", latency="low",
                        callback=callback) as stream:
        log.info("audio in: %s @ %.0f Hz, input latency %.1f ms", stream.device,
                 stream.samplerate, stream.latency * 1e3)
        while True:
            time.sleep(1.0)


def stream_file(blocks: "queue.Queue[np.ndarray | None]", config: FeatureConfig,
                path: Path, loop: bool) -> None:
    """Feed a sound file in at the real-time rate, as if it were live input."""
    import soundfile as sf

    audio, sr = sf.read(path, dtype="float64", always_2d=True)
    if sr != config.sample_rate:
        raise SystemExit(f"{path} is {sr} Hz; rerun with --sample-rate {sr}")
    mono = audio.mean(axis=1)
    log.info("replaying %s (%.1f s)%s", path, len(mono) / sr, " in a loop" if loop else "")
    next_time = time.perf_counter()
    while True:
        for start in range(0, len(mono) - config.hop + 1, config.hop):
            blocks.put(mono[start:start + config.hop])
            next_time += config.hop_s
            time.sleep(max(0.0, next_time - time.perf_counter()))
        if not loop:
            return


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--list-devices", action="store_true", help="print audio devices and exit")
    parser.add_argument("--device", help="input device name substring or index")
    parser.add_argument("--channel", type=int, default=0, help="0-based input channel of the guitar")
    parser.add_argument("--file", type=Path, help="replay a sound file instead of live input")
    parser.add_argument("--loop", action="store_true", help="with --file: loop forever")
    parser.add_argument("--sample-rate", type=float, default=FeatureConfig.sample_rate)
    parser.add_argument("--bands", type=int, default=FeatureConfig.n_bands, help="number of row strips")
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
    params = ParamServer(MappingParams(), "127.0.0.1", args.param_port)
    client = udp_client.UDPClient(args.osc_host, args.osc_port)
    log.info("sending OSC to %s:%d (%d strips)", args.osc_host, args.osc_port, config.n_bands)

    blocks: "queue.Queue[np.ndarray | None]" = queue.Queue(maxsize=QUEUE_MAX_BLOCKS)
    worker = threading.Thread(target=run_worker, args=(blocks, config, params, client), daemon=True)
    worker.start()
    try:
        if args.file:
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
