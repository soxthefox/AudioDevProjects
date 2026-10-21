"""What every input protocol decodes to."""

from dataclasses import dataclass

DMX_MAX = 512  # channels per universe


@dataclass(frozen=True)
class DmxData:
    universe: int  # Art-Net 15-bit port-address, or sACN universe
    data: bytes  # channel 1 first


@dataclass(frozen=True)
class Sync:
    """The sender says every universe of this frame has been sent."""
