"""Load the golden vectors and turn their JSON meaning into codec messages."""

import json
from pathlib import Path

from stinger_proto import (
    Config,
    ConfigKey,
    ConfigMac,
    Frame,
    Hello,
    Message,
    Packet,
    Ping,
    Pong,
    Show,
    Stats,
)

VECTORS_DIR = Path(__file__).resolve().parents[1] / "protocol" / "vectors"
VECTORS = json.loads((VECTORS_DIR / "vectors.json").read_text())


def _mac(text: str) -> bytes:
    return bytes.fromhex(text.replace(":", ""))


def message_from_json(m: dict) -> Message:
    match m["type"]:
        case "FRAME":
            return Frame(m["bar_id"], bytes.fromhex(m["rgb"]))
        case "SHOW":
            return Show(m["frame_seq"])
        case "CONFIG":
            return Config(m["bar_id"], ConfigKey[m["key"]], m["value"])
        case "CONFIG_MAC":
            return ConfigMac(_mac(m["mac"]), ConfigKey[m["key"]], m["value"])
        case "PING":
            return Ping(m["t_ms"])
        case "PONG":
            return Pong(m["t_ms"])
        case "HELLO":
            return Hello(m["bar_id"], _mac(m["mac"]), m["led_count"], m["fw_version"], m["rssi"])
        case "STATS":
            return Stats(m["rx_packets"], m["crc_errors"], m["tx_failures"])
    raise ValueError(m["type"])


def packet_from_json(m: dict) -> Packet:
    return Packet(m["seq"], message_from_json(m))
