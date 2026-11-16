// Bar logic against the shared scenarios (tests/bar_scenarios.json), plus ID storage.
#include <unity.h>

#include "bar_logic.h"
#include "bar_scenarios.h"
#include "fake_transport.h"

using namespace stinger;

namespace {

const BarConfig kConfig = {SCENARIO_LED_COUNT,           SCENARIO_HARD_LIMIT,
                           SCENARIO_FAILSAFE_TIMEOUT_MS, SCENARIO_FAILSAFE_FADE_MS,
                           SCENARIO_HELLO_INTERVAL_MS,   SCENARIO_FW_VERSION};

class MemoryIdStore : public IdStore {
  public:
    explicit MemoryIdStore(uint8_t id) : id(id) {}
    uint8_t load() override { return id; }
    void save(uint8_t newId) override {
        id = newId;
        saves++;
    }
    uint8_t id;
    int saves = 0;
};

void runScenario(const scenario_t &sc) {
    FakeTransport link;
    MemoryIdStore ids(sc.bar_id);
    BarLogic bar(link, ids, sc.mac, kConfig);
    bar.begin();
    for (size_t i = 0; i < sc.step_count; i++) {
        const scenario_step_t &step = sc.steps[i];
        char where[96];
        snprintf(where, sizeof(where), "%s, step %zu at %u ms", sc.name, i,
                 static_cast<unsigned>(step.at));
        switch (step.kind) {
        case STEP_SEND:
            bar.receive(step.bytes, step.len, step.at);
            break;
        case STEP_EXPECT_PIXELS: {
            uint8_t rgb[3 * SCENARIO_LED_COUNT];
            bar.render(step.at, rgb);
            TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(step.bytes, rgb, step.len, where);
            break;
        }
        case STEP_EXPECT_HELLO: {
            stinger_msg_t hello;
            bool due = bar.pollHello(step.at, &hello);
            TEST_ASSERT_EQUAL_MESSAGE(step.hello_id >= 0, due, where);
            if (due) {
                TEST_ASSERT_EQUAL_UINT8_MESSAGE(step.hello_id, hello.u.hello.bar_id, where);
            }
            break;
        }
        }
    }
}

const uint8_t kMac[6] = {0x02, 0x53, 0x54, 0x00, 0x00, 0x03};

} // namespace

void setUp() {}
void tearDown() {}

void test_shared_scenarios() {
    for (const scenario_t &sc : scenarios) {
        runScenario(sc);
    }
}

void test_assigned_id_is_saved_and_loaded_at_boot() {
    FakeTransport link;
    MemoryIdStore ids(STINGER_BAR_UNASSIGNED);
    {
        BarLogic bar(link, ids, kMac, kConfig);
        bar.begin();
        stinger_msg_t msg = {};
        msg.type = STINGER_MSG_CONFIG_MAC;
        msg.u.config_mac = {kMac, STINGER_KEY_ASSIGN_ID, 7};
        link.inject(encodePacket(msg));
    }
    TEST_ASSERT_EQUAL_UINT8(7, ids.id);
    TEST_ASSERT_EQUAL_INT(1, ids.saves);
    BarLogic rebooted(link, ids, kMac, kConfig);
    rebooted.begin();
    TEST_ASSERT_EQUAL_UINT8(7, rebooted.barId());
}

void test_corrupt_stored_id_boots_unassigned() {
    FakeTransport link;
    MemoryIdStore ids(200);
    BarLogic bar(link, ids, kMac, kConfig);
    bar.begin();
    TEST_ASSERT_EQUAL_UINT8(STINGER_BAR_UNASSIGNED, bar.barId());
}

void test_loop_broadcasts_hello_when_due() {
    FakeTransport link;
    MemoryIdStore ids(4);
    BarLogic bar(link, ids, kMac, kConfig);
    bar.begin();
    bar.loop(0);
    bar.loop(10);
    bar.loop(SCENARIO_HELLO_INTERVAL_MS);
    TEST_ASSERT_EQUAL_size_t(2, link.sent.size());
    TEST_ASSERT_TRUE(link.sent[0].to.isBroadcast());
    stinger_msg_t hello;
    TEST_ASSERT_EQUAL_INT(
        STINGER_OK, stinger_decode(link.sent[0].packet.data(), link.sent[0].packet.size(), &hello));
    TEST_ASSERT_EQUAL_UINT8(STINGER_MSG_HELLO, hello.type);
    TEST_ASSERT_EQUAL_UINT8(4, hello.u.hello.bar_id);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(kMac, hello.u.hello.mac, 6);
    TEST_ASSERT_EQUAL_INT8(0, hello.u.hello.rssi);
}

void test_failsafe_survives_millis_wraparound() {
    FakeTransport link;
    MemoryIdStore ids(3);
    BarLogic bar(link, ids, kMac, kConfig);
    bar.begin();
    const uint32_t nearWrap = 0xFFFFFF00u;
    std::vector<uint8_t> rgb(3 * SCENARIO_LED_COUNT, 0xFF);
    stinger_msg_t frame = {};
    frame.type = STINGER_MSG_FRAME;
    frame.u.frame = {3, SCENARIO_LED_COUNT, rgb.data()};
    stinger_msg_t show = {};
    show.type = STINGER_MSG_SHOW;
    auto f = encodePacket(frame), s = encodePacket(show);
    bar.receive(f.data(), f.size(), nearWrap);
    bar.receive(s.data(), s.size(), nearWrap);
    uint8_t out[3 * SCENARIO_LED_COUNT];
    bar.render(nearWrap + 0x200, out); // millis() has wrapped; 512 ms later
    TEST_ASSERT_EQUAL_UINT8(SCENARIO_HARD_LIMIT, out[0]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_shared_scenarios);
    RUN_TEST(test_assigned_id_is_saved_and_loaded_at_boot);
    RUN_TEST(test_corrupt_stored_id_boots_unassigned);
    RUN_TEST(test_loop_broadcasts_hello_when_due);
    RUN_TEST(test_failsafe_survives_millis_wraparound);
    return UNITY_END();
}
