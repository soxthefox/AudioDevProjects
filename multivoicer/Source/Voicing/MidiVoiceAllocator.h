#pragma once

#include <array>
#include <vector>

namespace mv {

class MidiVoiceAllocator {
public:
    static constexpr int kMaxSlots = 4;

    struct Slot {
        bool active = false;
        int  midiNote = -1;
        int  age = 0;
    };

    void setPoly(int numSlots);
    void setMono();
    void clearAll();
    void noteOn(int midiNote);
    void noteOff(int midiNote);
    void tick();   // call once per block — ages all active slots
    std::array<Slot, kMaxSlots> snapshot() const { return slots; }

private:
    std::array<Slot, kMaxSlots> slots {};
    bool                        mono = false;
    int                         polyCount = 4;
    std::vector<int>            monoStack;
};

} // namespace mv
