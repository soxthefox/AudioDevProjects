#include <catch2/catch_test_macros.hpp>
#include "Voicing/MidiVoiceAllocator.h"

TEST_CASE("Poly: first note goes to slot 0", "[allocator]") {
    mv::MidiVoiceAllocator a;
    a.setPoly(4);
    a.noteOn(60);
    auto s = a.snapshot();
    CHECK(s[0].active);
    CHECK(s[0].midiNote == 60);
    CHECK_FALSE(s[1].active);
}

TEST_CASE("Poly: fifth note steals oldest", "[allocator]") {
    mv::MidiVoiceAllocator a;
    a.setPoly(4);
    a.noteOn(60); a.tick();
    a.noteOn(62); a.tick();
    a.noteOn(64); a.tick();
    a.noteOn(65); a.tick();
    a.noteOn(67);  // 5th — steals slot containing 60 (oldest)
    auto s = a.snapshot();
    for (auto& slot : s) CHECK(slot.active);
    bool hasNote60 = false; for (auto& slot : s) if (slot.midiNote == 60) hasNote60 = true;
    bool hasNote67 = false; for (auto& slot : s) if (slot.midiNote == 67) hasNote67 = true;
    CHECK_FALSE(hasNote60);
    CHECK(hasNote67);
}

TEST_CASE("Poly: note-off frees slot", "[allocator]") {
    mv::MidiVoiceAllocator a;
    a.setPoly(4);
    a.noteOn(60);
    a.noteOff(60);
    auto s = a.snapshot();
    CHECK_FALSE(s[0].active);
}

TEST_CASE("Poly: stolen note's note-off is ignored", "[allocator]") {
    mv::MidiVoiceAllocator a;
    a.setPoly(4);
    a.noteOn(60); a.tick();
    a.noteOn(62); a.tick();
    a.noteOn(64); a.tick();
    a.noteOn(65); a.tick();
    a.noteOn(67);
    a.noteOff(60);  // 60 was stolen — ignore
    auto s = a.snapshot();
    int active = 0; for (auto& slot : s) if (slot.active) ++active;
    CHECK(active == 4);
}

TEST_CASE("Mono: latest note wins, legato on stack pop", "[allocator]") {
    mv::MidiVoiceAllocator a;
    a.setMono();
    a.noteOn(60);
    auto s1 = a.snapshot();
    CHECK(s1[0].active); CHECK(s1[0].midiNote == 60);

    a.noteOn(64);
    auto s2 = a.snapshot();
    CHECK(s2[0].active); CHECK(s2[0].midiNote == 64);

    a.noteOff(64);
    auto s3 = a.snapshot();
    CHECK(s3[0].active); CHECK(s3[0].midiNote == 60);  // legato fallback

    a.noteOff(60);
    auto s4 = a.snapshot();
    CHECK_FALSE(s4[0].active);
}
