"""The bridge's live console view: bars from HELLO, gateway STATS, timing."""

from stinger_bridge.app import Bridge


def _ms(value: float | None) -> str:
    return "-" if value is None else f"{value:.1f} ms"


def render(bridge: Bridge, now_ms: int, fps: float) -> str:
    s = bridge.gateway_stats
    lines = [
        f"out {bridge.link}   frames {bridge.frames_sent} ({fps:.1f} fps)   "
        f"input->SHOW p50 {_ms(bridge.latency.percentile(50))} "
        f"p99 {_ms(bridge.latency.percentile(99))}",
        "gateway "
        + (
            "no STATS yet"
            if s is None
            else f"rx {s.rx_packets}  crc errors {s.crc_errors}  radio failures {s.tx_failures}"
        )
        + f"   ping {_ms(bridge.rtt_ms)}",
        "",
        " ID  MAC                LEDs  fw    RSSI  status",
    ]
    bars = bridge.registry.bars(now_ms)
    for b in bars:
        e = b.entry
        state = "online" if b.online else f"OFFLINE ({b.silent_ms // 1000} s)"
        lines.append(
            f"{e.bar_id or '--':>3}  {e.mac.hex(':')}  {e.led_count:>4}  "
            f"{e.fw_version >> 8}.{e.fw_version & 0xFF:<3} {e.rssi:>5}  {state}"
        )
    if not bars:
        lines.append("  (no bars heard yet)")
    return "\n".join(lines)
