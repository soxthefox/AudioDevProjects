"""Decode errors, named as in PROTOCOL.md."""

from enum import Enum


class ErrorKind(Enum):
    TOO_SHORT = "TOO_SHORT"
    UNKNOWN_TYPE = "UNKNOWN_TYPE"
    BAD_LENGTH = "BAD_LENGTH"
    BAD_FIELD = "BAD_FIELD"
    FRAMING = "FRAMING"
    CRC = "CRC"


class DecodeError(ValueError):
    def __init__(self, kind: ErrorKind, detail: str = "") -> None:
        super().__init__(f"{kind.value}: {detail}" if detail else kind.value)
        self.kind = kind
