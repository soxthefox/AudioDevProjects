"""The bridge's bar list: online until 3 s without HELLO."""

from stinger_bridge.registry import BarRegistry
from stinger_proto import Hello

MAC = bytes.fromhex("025354000003")


def test_bar_goes_offline_after_silence_and_back_on_hello() -> None:
    registry = BarRegistry(offline_after_ms=3000)
    registry.hello(Hello(3, MAC, 60, 0x0100, -40), now_ms=1000)
    assert registry.online_ids(3999) == {3}
    assert registry.online_ids(4000) == set()
    (status,) = registry.bars(4500)
    assert not status.online and status.silent_ms == 3500
    registry.hello(Hello(3, MAC, 60, 0x0100, -41), now_ms=4600)
    assert registry.online_ids(4600) == {3}


def test_reassigned_bar_is_listed_once_under_its_new_id() -> None:
    registry = BarRegistry(offline_after_ms=3000)
    registry.hello(Hello(0, MAC, 60, 0x0100, -40), now_ms=0)
    registry.hello(Hello(5, MAC, 60, 0x0100, -40), now_ms=10)
    assert [s.entry.bar_id for s in registry.bars(20)] == [5]


def test_sorted_by_id_with_unassigned_last() -> None:
    registry = BarRegistry(offline_after_ms=3000)
    for bar_id, last in ((0, 1), (7, 2), (2, 3)):
        registry.hello(Hello(bar_id, bytes([2, 0, 0, 0, 0, last]), 60, 0, 0), now_ms=0)
    assert [s.entry.bar_id for s in registry.bars(0)] == [2, 7, 0]
