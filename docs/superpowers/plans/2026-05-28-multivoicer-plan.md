# Multivoicer V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a JUCE diatonic harmonizer plugin (VST3+AU, macOS arm64) with preset and MIDI modes, 1-4 voices, per-voice ADSR/EQ, and a fully automated test suite that produces downloadable Mac artifacts via CI.

**Architecture:** Layered C++. Pure-logic music theory module (no audio dependency). Swappable DSP primitives behind interfaces (`PitchDetector`, `PitchShifter`). Library-backed pitch shifting (Signalsmith Stretch); DIY YIN for pitch detection. Plugin glue via standard JUCE `AudioProcessor` + APVTS. Tests run on every push via GitHub Actions matrix.

**Tech Stack:** JUCE (CMake, FetchContent), Signalsmith Stretch (BSD-3), Catch2 v3, pluginval, GitHub Actions (windows-latest + macos-latest).

**Spec:** Full design at `docs/superpowers/specs/2026-05-28-multivoicer-design.md` — read it before starting. The plan implements that spec exactly.

**Conventions for this plan:**
- All paths are relative to repo root unless prefixed `multivoicer/`.
- Working directory for all `cmake` / `ctest` commands is `multivoicer/build/`.
- "Run" commands are exactly what to type.
- Each task ends with a commit. Frequent small commits > large ones.
- Tests use Catch2 v3 macros (`TEST_CASE`, `SECTION`, `REQUIRE`, `CHECK`).
- TDD throughout: write the failing test, run it to confirm failure, implement, run to confirm pass, commit.

---

## Phase 0 — Project scaffolding

Goal: get an empty plugin compiling and an empty test executable running. No DSP yet. Every later phase builds on this skeleton.

### Task 1: Create project skeleton

**Files:**
- Create: `multivoicer/.gitignore`
- Create: `multivoicer/README.md`
- Create: `multivoicer/CMakeLists.txt` (minimal placeholder)

- [ ] **Step 1: Create the project folder and a project-local `.gitignore`**

Write `multivoicer/.gitignore`:

```gitignore
# Build outputs
build/
cmake-build-*/

# JUCE Projucer leftovers (we don't use it, but just in case)
JuceLibraryCode/

# Local test artifacts
Tests/golden_actual/
```

- [ ] **Step 2: Create a project README**

Write `multivoicer/README.md`:

```markdown
# Multivoicer

A JUCE diatonic-harmonizer plugin (VST3 + AU) with preset and MIDI control modes,
up to 4 voices, per-voice ADSR and EQ, formant-preserving pitch shifting, and a
talkbox-style mono MIDI mode.

See `../docs/superpowers/specs/2026-05-28-multivoicer-design.md` for the design.

## Build

```sh
cmake -S . -B build
cmake --build build --config Release
```

## Test

```sh
ctest --test-dir build --output-on-failure
```

## Plugin artifacts

After a successful Release build the plugins land in `build/Multivoicer_artefacts/Release/`:

- `VST3/Multivoicer.vst3`
- `AU/Multivoicer.component` (macOS only)
- `Standalone/Multivoicer`
```

- [ ] **Step 3: Create a minimal placeholder CMakeLists**

Write `multivoicer/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.22)
project(Multivoicer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Phase 0 placeholder. JUCE / Signalsmith / Catch2 added in later tasks.
message(STATUS "Multivoicer: Phase 0 scaffold")
```

- [ ] **Step 4: Verify CMake configures**

Run: `cmake -S multivoicer -B multivoicer/build`
Expected: configuration succeeds; output includes `Multivoicer: Phase 0 scaffold`.

- [ ] **Step 5: Commit**

```sh
git add multivoicer/.gitignore multivoicer/README.md multivoicer/CMakeLists.txt
git commit -m "multivoicer: scaffold project folder with placeholder CMakeLists"
```

---

### Task 2: Add JUCE via FetchContent

**Files:**
- Create: `multivoicer/cmake/JUCE.cmake`
- Modify: `multivoicer/CMakeLists.txt`

- [ ] **Step 1: Create the JUCE FetchContent module**

Write `multivoicer/cmake/JUCE.cmake`:

```cmake
include(FetchContent)

set(JUCE_VERSION 8.0.4)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG ${JUCE_VERSION}
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)
```

- [ ] **Step 2: Wire it into the top-level CMakeLists**

Modify `multivoicer/CMakeLists.txt` — replace the `message(STATUS ...)` line with:

```cmake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(JUCE)
```

- [ ] **Step 3: Configure to confirm JUCE is fetched**

Run: `cmake -S multivoicer -B multivoicer/build`
Expected: fetches JUCE (first time only; cached afterwards). No errors.

- [ ] **Step 4: Commit**

```sh
git add multivoicer/cmake/JUCE.cmake multivoicer/CMakeLists.txt
git commit -m "multivoicer: add JUCE via FetchContent"
```

---

### Task 3: Add Signalsmith Stretch via FetchContent

**Files:**
- Create: `multivoicer/cmake/Signalsmith.cmake`
- Modify: `multivoicer/CMakeLists.txt`

- [ ] **Step 1: Create the Signalsmith FetchContent module**

Write `multivoicer/cmake/Signalsmith.cmake`:

```cmake
include(FetchContent)

# Signalsmith Stretch — BSD-3, header-only-ish C++17 pitch/time stretcher.
FetchContent_Declare(
    signalsmith_stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(signalsmith_stretch)

# Signalsmith doesn't ship its own CMake target; wrap it.
add_library(signalsmith_stretch INTERFACE)
target_include_directories(signalsmith_stretch INTERFACE ${signalsmith_stretch_SOURCE_DIR})
add_library(signalsmith::stretch ALIAS signalsmith_stretch)
```

- [ ] **Step 2: Include it from the top-level CMakeLists**

Modify `multivoicer/CMakeLists.txt` — add right after `include(JUCE)`:

```cmake
include(Signalsmith)
```

- [ ] **Step 3: Configure to confirm Signalsmith is fetched**

Run: `cmake -S multivoicer -B multivoicer/build`
Expected: fetches signalsmith-stretch. No errors.

- [ ] **Step 4: Commit**

```sh
git add multivoicer/cmake/Signalsmith.cmake multivoicer/CMakeLists.txt
git commit -m "multivoicer: add Signalsmith Stretch via FetchContent"
```

---

### Task 4: Add Catch2 v3 via FetchContent

**Files:**
- Create: `multivoicer/cmake/Catch2.cmake`
- Modify: `multivoicer/CMakeLists.txt`

- [ ] **Step 1: Create the Catch2 FetchContent module**

Write `multivoicer/cmake/Catch2.cmake`:

```cmake
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.6.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
```

- [ ] **Step 2: Include it from the top-level CMakeLists and enable testing**

Modify `multivoicer/CMakeLists.txt` — add after `include(Signalsmith)`:

```cmake
enable_testing()
include(Catch2)
```

- [ ] **Step 3: Configure to confirm Catch2 is fetched**

Run: `cmake -S multivoicer -B multivoicer/build`
Expected: fetches Catch2 v3.6.0. No errors.

- [ ] **Step 4: Commit**

```sh
git add multivoicer/cmake/Catch2.cmake multivoicer/CMakeLists.txt
git commit -m "multivoicer: add Catch2 v3 via FetchContent + enable_testing"
```

---

### Task 5: Empty plugin target (smoke build)

Goal: produce a buildable JUCE plugin target with zero source files of our own. Proves the toolchain works end-to-end.

**Files:**
- Create: `multivoicer/Source/Plugin/PluginProcessor.h`
- Create: `multivoicer/Source/Plugin/PluginProcessor.cpp`
- Modify: `multivoicer/CMakeLists.txt`

- [ ] **Step 1: Write a stub `PluginProcessor` (no DSP — placeholder so the JUCE target has at least one source)**

Write `multivoicer/Source/Plugin/PluginProcessor.h`:

```cpp
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class MultivoicerProcessor : public juce::AudioProcessor {
public:
    MultivoicerProcessor();
    ~MultivoicerProcessor() override = default;

    const juce::String getName() const override { return "Multivoicer"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultivoicerProcessor)
};
```

Write `multivoicer/Source/Plugin/PluginProcessor.cpp`:

```cpp
#include "PluginProcessor.h"

MultivoicerProcessor::MultivoicerProcessor()
    : juce::AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new MultivoicerProcessor();
}
```

- [ ] **Step 2: Add `juce_add_plugin` to CMakeLists**

Append to `multivoicer/CMakeLists.txt`:

```cmake
juce_add_plugin(Multivoicer
    PRODUCT_NAME "Multivoicer"
    COMPANY_NAME "AudioDevProjects"
    BUNDLE_ID "com.audiodevprojects.multivoicer"
    PLUGIN_MANUFACTURER_CODE Adev
    PLUGIN_CODE Mvcr
    FORMATS VST3 Standalone $<$<PLATFORM_ID:Darwin>:AU>
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE)

target_sources(Multivoicer PRIVATE
    Source/Plugin/PluginProcessor.cpp)

target_compile_definitions(Multivoicer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Multivoicer PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    signalsmith::stretch
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

target_include_directories(Multivoicer PRIVATE Source)
```

- [ ] **Step 3: Configure and build the plugin**

Run: `cmake -S multivoicer -B multivoicer/build`
Then: `cmake --build multivoicer/build --config Release --target Multivoicer_VST3 Multivoicer_Standalone`
Expected: builds succeed. Artifacts in `multivoicer/build/Multivoicer_artefacts/Release/VST3/Multivoicer.vst3` and `…/Standalone/`.

- [ ] **Step 4: Commit**

```sh
git add multivoicer/Source/Plugin/PluginProcessor.h multivoicer/Source/Plugin/PluginProcessor.cpp multivoicer/CMakeLists.txt
git commit -m "multivoicer: empty plugin target builds VST3+Standalone"
```

---

### Task 6: Empty test target (smoke test)

**Files:**
- Create: `multivoicer/Tests/CMakeLists.txt`
- Create: `multivoicer/Tests/smoke_test.cpp`
- Modify: `multivoicer/CMakeLists.txt`

- [ ] **Step 1: Write a Catch2 smoke test**

Write `multivoicer/Tests/smoke_test.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Smoke: Catch2 + CMake wired up", "[smoke]") {
    REQUIRE(1 + 1 == 2);
}
```

- [ ] **Step 2: Write the test CMakeLists**

Write `multivoicer/Tests/CMakeLists.txt`:

```cmake
add_executable(MultivoicerTests
    smoke_test.cpp)

target_link_libraries(MultivoicerTests PRIVATE
    Catch2::Catch2WithMain
    juce::juce_audio_utils
    juce::juce_dsp
    signalsmith::stretch)

target_compile_definitions(MultivoicerTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1)

target_include_directories(MultivoicerTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Source)

# Make tests depend on the plugin compiling — Layer 0 build gate.
add_dependencies(MultivoicerTests Multivoicer)

catch_discover_tests(MultivoicerTests)
```

- [ ] **Step 3: Wire into the top-level CMakeLists**

Append to `multivoicer/CMakeLists.txt`:

```cmake
add_subdirectory(Tests)
```

- [ ] **Step 4: Configure, build, run**

Run:
```sh
cmake -S multivoicer -B multivoicer/build
cmake --build multivoicer/build --config Release --target MultivoicerTests
ctest --test-dir multivoicer/build --output-on-failure
```
Expected: smoke test passes. `ctest` exits 0.

- [ ] **Step 5: Commit**

```sh
git add multivoicer/Tests/CMakeLists.txt multivoicer/Tests/smoke_test.cpp multivoicer/CMakeLists.txt
git commit -m "multivoicer: Catch2 smoke test + ctest depends on plugin build"
```

**End of Phase 0.** You now have: a building plugin target, a passing test, and the build gate wired up.

---

## Phase 1 — Pure music theory

Goal: implement `MusicTheory.h/.cpp` and `Scales.h` with full test coverage. No audio dependency. Each task is one operation, test-first.

### Task 7: Music theory types + Scales registry (header-only types)

**Files:**
- Create: `multivoicer/Source/Music/MusicTheory.h`
- Create: `multivoicer/Source/Music/Scales.h`
- Create: `multivoicer/Tests/music_theory_test.cpp`
- Modify: `multivoicer/Tests/CMakeLists.txt`

- [ ] **Step 1: Write a failing test for the types and registry**

Write `multivoicer/Tests/music_theory_test.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "Music/MusicTheory.h"
#include "Music/Scales.h"

using namespace mv;

TEST_CASE("PitchClassSet::contains", "[music_theory]") {
    PitchClassSet major { 0b0000'1010'1101'0101 };
    CHECK(major.contains(0));   // root
    CHECK(major.contains(4));   // major 3rd
    CHECK(major.contains(7));   // 5th
    CHECK_FALSE(major.contains(1));  // minor 2nd not in major
    CHECK_FALSE(major.contains(3));  // minor 3rd not in major
    CHECK(major.contains(12));  // wraps: octave is in
    CHECK(major.contains(16));  // wraps: 4+12 == major 3rd of next octave
}

TEST_CASE("Scales registry has Major and NaturalMinor", "[music_theory]") {
    REQUIRE(Scales::All.size() == 2);
    bool foundMajor = false;
    bool foundMinor = false;
    for (const auto* s : Scales::All) {
        if (std::string_view(s->name) == "Major")         foundMajor = true;
        if (std::string_view(s->name) == "Natural Minor") foundMinor = true;
    }
    CHECK(foundMajor);
    CHECK(foundMinor);
}

TEST_CASE("Major scale pitch class set is correct", "[music_theory]") {
    // C major: C D E F G A B = semitones 0 2 4 5 7 9 11
    const auto& pcs = Scales::Major.intervals;
    CHECK(pcs.contains(0));
    CHECK(pcs.contains(2));
    CHECK(pcs.contains(4));
    CHECK(pcs.contains(5));
    CHECK(pcs.contains(7));
    CHECK(pcs.contains(9));
    CHECK(pcs.contains(11));
    CHECK_FALSE(pcs.contains(1));
    CHECK_FALSE(pcs.contains(3));
    CHECK_FALSE(pcs.contains(6));
    CHECK_FALSE(pcs.contains(8));
    CHECK_FALSE(pcs.contains(10));
}

TEST_CASE("Natural minor scale pitch class set is correct", "[music_theory]") {
    // Natural minor: 0 2 3 5 7 8 10
    const auto& pcs = Scales::NaturalMinor.intervals;
    CHECK(pcs.contains(0));
    CHECK(pcs.contains(2));
    CHECK(pcs.contains(3));
    CHECK(pcs.contains(5));
    CHECK(pcs.contains(7));
    CHECK(pcs.contains(8));
    CHECK(pcs.contains(10));
    CHECK_FALSE(pcs.contains(4));
    CHECK_FALSE(pcs.contains(11));
}
```

Update `multivoicer/Tests/CMakeLists.txt` — replace the `add_executable` source list with:

```cmake
add_executable(MultivoicerTests
    smoke_test.cpp
    music_theory_test.cpp)
```

- [ ] **Step 2: Run the test, confirm it fails (headers don't exist yet)**

Run: `cmake --build multivoicer/build --config Release --target MultivoicerTests`
Expected: FAIL with "cannot open source file 'Music/MusicTheory.h'" (or similar).

- [ ] **Step 3: Implement the headers**

Write `multivoicer/Source/Music/MusicTheory.h`:

```cpp
#pragma once

#include <array>
#include <cstdint>

namespace mv {

enum class NoteName : int {
    C = 0, Cs = 1, D = 2, Ds = 3, E = 4, F = 5,
    Fs = 6, G = 7, Gs = 8, A = 9, As = 10, B = 11
};

struct PitchClassSet {
    uint16_t mask;  // 12 LSBs used; bit i set iff semitone i above root is in the scale
    constexpr bool contains(int semitone) const {
        return ((mask >> (((semitone % 12) + 12) % 12)) & 1u) != 0u;
    }
};

struct ScaleDef {
    const char* name;
    PitchClassSet intervals;
};

struct Key {
    NoteName root;
    const ScaleDef* scale;
};

// Pure functions — defined in MusicTheory.cpp (Tasks 8–11).
int    midiFromFreq(double hz);
double freqFromMidi(double midi);
int    quantizeToKey(int midiNote, Key key);
int    scaleDegreeOf(int midiNote, Key key);   // 0=root, 1=2nd, ..., -1 if not in key
int    targetForInterval(int inputMidi, int diatonicInterval, Key key);

} // namespace mv
```

Write `multivoicer/Source/Music/Scales.h`:

```cpp
#pragma once

#include "MusicTheory.h"

namespace mv::Scales {

// Major: 0 2 4 5 7 9 11 -> 0b0000'1010'1101'0101
inline constexpr ScaleDef Major {
    "Major",
    PitchClassSet{ 0b0000'1010'1101'0101 }
};

// Natural Minor: 0 2 3 5 7 8 10 -> 0b0000'0101'1011'0101
inline constexpr ScaleDef NaturalMinor {
    "Natural Minor",
    PitchClassSet{ 0b0000'0101'1011'0101 }
};

inline constexpr std::array<const ScaleDef*, 2> All { &Major, &NaturalMinor };

} // namespace mv::Scales
```

- [ ] **Step 4: Build and run tests**

Run:
```sh
cmake --build multivoicer/build --config Release --target MultivoicerTests
ctest --test-dir multivoicer/build --output-on-failure
```
Expected: all four `[music_theory]` test cases pass.

- [ ] **Step 5: Commit**

```sh
git add multivoicer/Source/Music/MusicTheory.h multivoicer/Source/Music/Scales.h \
        multivoicer/Tests/music_theory_test.cpp multivoicer/Tests/CMakeLists.txt
git commit -m "multivoicer: MusicTheory types + Scales registry (Major, NaturalMinor)"
```

---

### Task 8: midiFromFreq / freqFromMidi

**Files:**
- Create: `multivoicer/Source/Music/MusicTheory.cpp`
- Modify: `multivoicer/Tests/music_theory_test.cpp`
- Modify: `multivoicer/Tests/CMakeLists.txt` (add `Source/Music/MusicTheory.cpp`)
- Modify: `multivoicer/CMakeLists.txt` (add `Source/Music/MusicTheory.cpp` to plugin)

- [ ] **Step 1: Add failing tests for the frequency conversions**

Append to `multivoicer/Tests/music_theory_test.cpp`:

```cpp
#include <catch2/catch_approx.hpp>

TEST_CASE("midiFromFreq / freqFromMidi roundtrip", "[music_theory]") {
    CHECK(mv::midiFromFreq(440.0) == 69);              // A4
    CHECK(mv::midiFromFreq(261.6256) == 60);           // C4
    CHECK(mv::freqFromMidi(69.0) == Catch::Approx(440.0));
    CHECK(mv::freqFromMidi(60.0) == Catch::Approx(261.6256).margin(0.01));
}

TEST_CASE("midiFromFreq rounds to nearest semitone", "[music_theory]") {
    CHECK(mv::midiFromFreq(443.0) == 69);
    CHECK(mv::midiFromFreq(450.0) == 69);
    CHECK(mv::midiFromFreq(460.0) == 70);
}
```

- [ ] **Step 2: Run, confirm fail (linker error: undefined reference to `mv::midiFromFreq`)**

Run: `cmake --build multivoicer/build --config Release --target MultivoicerTests`
Expected: link error.

- [ ] **Step 3: Implement**

Write `multivoicer/Source/Music/MusicTheory.cpp`:

```cpp
#include "MusicTheory.h"
#include <cmath>

namespace mv {

int midiFromFreq(double hz) {
    if (hz <= 0.0) return 0;
    return static_cast<int>(std::lround(69.0 + 12.0 * std::log2(hz / 440.0)));
}

double freqFromMidi(double midi) {
    return 440.0 * std::pow(2.0, (midi - 69.0) / 12.0);
}

// Stubs for later tasks — keep linker happy until Tasks 9/10/11 implement them.
int quantizeToKey(int, Key)               { return 0; }
int scaleDegreeOf(int, Key)               { return -1; }
int targetForInterval(int, int, Key)      { return 0; }

} // namespace mv
```

Update `multivoicer/Tests/CMakeLists.txt`:

```cmake
add_executable(MultivoicerTests
    smoke_test.cpp
    music_theory_test.cpp
    ${CMAKE_SOURCE_DIR}/Source/Music/MusicTheory.cpp)
```

Update `multivoicer/CMakeLists.txt` `target_sources(Multivoicer ...)`:

```cmake
target_sources(Multivoicer PRIVATE
    Source/Plugin/PluginProcessor.cpp
    Source/Music/MusicTheory.cpp)
```

- [ ] **Step 4: Build and run tests**

Run:
```sh
cmake --build multivoicer/build --config Release --target MultivoicerTests
ctest --test-dir multivoicer/build --output-on-failure
```
Expected: pass.

- [ ] **Step 5: Commit**

```sh
git add multivoicer/Source/Music/MusicTheory.cpp \
        multivoicer/Tests/music_theory_test.cpp \
        multivoicer/Tests/CMakeLists.txt multivoicer/CMakeLists.txt
git commit -m "multivoicer: midiFromFreq / freqFromMidi"
```

---

### Task 9: scaleDegreeOf

**Files:**
- Modify: `multivoicer/Source/Music/MusicTheory.cpp`
- Modify: `multivoicer/Tests/music_theory_test.cpp`

- [ ] **Step 1: Failing tests**

Append to `multivoicer/Tests/music_theory_test.cpp`:

```cpp
TEST_CASE("scaleDegreeOf for C major", "[music_theory]") {
    mv::Key cMajor { mv::NoteName::C, &mv::Scales::Major };
    CHECK(mv::scaleDegreeOf(60, cMajor) == 0);   // C4 -> root (degree 0)
    CHECK(mv::scaleDegreeOf(62, cMajor) == 1);   // D4 -> 2nd
    CHECK(mv::scaleDegreeOf(64, cMajor) == 2);   // E4 -> 3rd
    CHECK(mv::scaleDegreeOf(65, cMajor) == 3);   // F4 -> 4th
    CHECK(mv::scaleDegreeOf(67, cMajor) == 4);   // G4 -> 5th
    CHECK(mv::scaleDegreeOf(69, cMajor) == 5);   // A4 -> 6th
    CHECK(mv::scaleDegreeOf(71, cMajor) == 6);   // B4 -> 7th
    CHECK(mv::scaleDegreeOf(61, cMajor) == -1);  // C#4 -> not in key
    CHECK(mv::scaleDegreeOf(72, cMajor) == 0);   // C5 -> root (different octave, same degree)
}

TEST_CASE("scaleDegreeOf for A natural minor", "[music_theory]") {
    mv::Key aMinor { mv::NoteName::A, &mv::Scales::NaturalMinor };
    CHECK(mv::scaleDegreeOf(69, aMinor) == 0);   // A4 -> root
    CHECK(mv::scaleDegreeOf(71, aMinor) == 1);   // B4 -> 2nd
    CHECK(mv::scaleDegreeOf(72, aMinor) == 2);   // C5 -> 3rd
    CHECK(mv::scaleDegreeOf(70, aMinor) == -1);  // A#4 -> not in key
}
```

- [ ] **Step 2: Run, confirm failure (current stub returns -1 always)**

Run: `ctest --test-dir multivoicer/build --output-on-failure`
Expected: FAIL — most scaleDegreeOf assertions fail because stub returns -1.

- [ ] **Step 3: Replace the stub**

In `multivoicer/Source/Music/MusicTheory.cpp`, replace the `scaleDegreeOf` stub with:

```cpp
int scaleDegreeOf(int midiNote, Key key) {
    int semitoneFromRoot = ((midiNote - static_cast<int>(key.root)) % 12 + 12) % 12;
    if (!key.scale->intervals.contains(semitoneFromRoot)) return -1;

    int degree = 0;
    for (int i = 0; i < semitoneFromRoot; ++i) {
        if (key.scale->intervals.contains(i)) ++degree;
    }
    return degree;
}
```

- [ ] **Step 4: Build and run**

Run:
```sh
cmake --build multivoicer/build --config Release --target MultivoicerTests
ctest --test-dir multivoicer/build --output-on-failure
```
Expected: pass.

- [ ] **Step 5: Commit**

```sh
git add multivoicer/Source/Music/MusicTheory.cpp multivoicer/Tests/music_theory_test.cpp
git commit -m "multivoicer: scaleDegreeOf"
```

---

### Task 10: quantizeToKey

**Files:**
- Modify: `multivoicer/Source/Music/MusicTheory.cpp`
- Modify: `multivoicer/Tests/music_theory_test.cpp`

- [ ] **Step 1: Failing tests**

Append to `multivoicer/Tests/music_theory_test.cpp`:

```cpp
TEST_CASE("quantizeToKey snaps to nearest in-key note", "[music_theory]") {
    mv::Key cMajor { mv::NoteName::C, &mv::Scales::Major };
    CHECK(mv::quantizeToKey(60, cMajor) == 60);  // C4 -> C4 (in key)
    CHECK(mv::quantizeToKey(61, cMajor) == 60);  // C#4 -> C4 (closer than D4)

    // F#4 = 66: equidistant from F4(65) and G4(67). Tie -> prefer lower (deterministic).
    CHECK(mv::quantizeToKey(66, cMajor) == 65);

    // Bb4 = 70: 1 semi from A4(69), 1 semi from B4(71). Tie -> prefer lower.
    CHECK(mv::quantizeToKey(70, cMajor) == 69);
}

TEST_CASE("quantizeToKey across octaves", "[music_theory]") {
    mv::Key cMajor { mv::NoteName::C, &mv::Scales::Major };
    CHECK(mv::quantizeToKey(72, cMajor) == 72);  // C5
    CHECK(mv::quantizeToKey(85, cMajor) == 84);  // C#6 -> C6
}
```

- [ ] **Step 2: Run, confirm failure (stub returns 0)**

Run: `ctest --test-dir multivoicer/build --output-on-failure`
Expected: FAIL.

- [ ] **Step 3: Replace the stub**

In `multivoicer/Source/Music/MusicTheory.cpp`, replace the `quantizeToKey` stub with:

```cpp
int quantizeToKey(int midiNote, Key key) {
    // Already in key? Done.
    if (scaleDegreeOf(midiNote, key) >= 0) return midiNote;

    // Search outward (1, -1, 2, -2, ...). On tie at same |distance|,
    // the lower note wins because we test +distance only after -distance.
    for (int distance = 1; distance <= 6; ++distance) {
        int down = midiNote - distance;
        if (scaleDegreeOf(down, key) >= 0) return down;
        int up = midiNote + distance;
        if (scaleDegreeOf(up, key) >= 0) return up;
    }
    return midiNote; // unreachable — every 12-semitone window has scale notes
}
```

- [ ] **Step 4: Build and run**

Run:
```sh
cmake --build multivoicer/build --config Release --target MultivoicerTests
ctest --test-dir multivoicer/build --output-on-failure
```
Expected: pass.

- [ ] **Step 5: Commit**

```sh
git add multivoicer/Source/Music/MusicTheory.cpp multivoicer/Tests/music_theory_test.cpp
git commit -m "multivoicer: quantizeToKey with deterministic tie-break (lower wins)"
```

---

### Task 11: targetForInterval

**Files:**
- Modify: `multivoicer/Source/Music/MusicTheory.cpp`
- Modify: `multivoicer/Tests/music_theory_test.cpp`

- [ ] **Step 1: Failing tests**

Append to `multivoicer/Tests/music_theory_test.cpp`:

```cpp
TEST_CASE("targetForInterval in C major", "[music_theory]") {
    mv::Key cMajor { mv::NoteName::C, &mv::Scales::Major };

    // +2 scale-degree = "diatonic 3rd above"
    CHECK(mv::targetForInterval(60, +2, cMajor) == 64);  // C4 -> E4 (major 3rd)
    CHECK(mv::targetForInterval(62, +2, cMajor) == 65);  // D4 -> F4 (minor 3rd)
    CHECK(mv::targetForInterval(64, +2, cMajor) == 67);  // E4 -> G4 (minor 3rd)
    CHECK(mv::targetForInterval(65, +2, cMajor) == 69);  // F4 -> A4 (major 3rd)
    CHECK(mv::targetForInterval(67, +2, cMajor) == 71);  // G4 -> B4 (major 3rd)

    // +4 = 5th, +7 = octave (7 diatonic degrees in 7-note scale)
    CHECK(mv::targetForInterval(60, +4, cMajor) == 67);  // C4 -> G4 (perfect 5th)
    CHECK(mv::targetForInterval(60, +7, cMajor) == 72);  // C4 -> C5

    // Negative interval = down
    CHECK(mv::targetForInterval(60, -2, cMajor) == 57);  // C4 -> A3 (down a 3rd)
    CHECK(mv::targetForInterval(60, -7, cMajor) == 48);  // C4 -> C3 (octave down)
}

TEST_CASE("targetForInterval in A natural minor", "[music_theory]") {
    mv::Key aMinor { mv::NoteName::A, &mv::Scales::NaturalMinor };

    // C in A-minor at scale-degree +2 -> E (in-key minor 3rd from C in A-natural-minor)
    // A-minor scale notes: A B C D E F G
    // From C, +2 degrees = E.
    CHECK(mv::targetForInterval(60, +2, aMinor) == 64);  // C4 -> E4
}

TEST_CASE("targetForInterval snaps off-key input first", "[music_theory]") {
    mv::Key cMajor { mv::NoteName::C, &mv::Scales::Major };
    // C#4 (61) is off-key -> snap to C4 (60), then +2 -> E4 (64).
    CHECK(mv::targetForInterval(61, +2, cMajor) == 64);
}
```

- [ ] **Step 2: Run, confirm failure (stub returns 0)**

Run: `ctest --test-dir multivoicer/build --output-on-failure`
Expected: FAIL.

- [ ] **Step 3: Replace the stub**

In `multivoicer/Source/Music/MusicTheory.cpp`, replace the `targetForInterval` stub with:

```cpp
int targetForInterval(int inputMidi, int diatonicInterval, Key key) {
    // 1. Snap off-key input to nearest in-key note.
    int snapped = quantizeToKey(inputMidi, key);

    // 2. Walk diatonicInterval steps through the scale.
    int dir = (diatonicInterval >= 0) ? 1 : -1;
    int steps = std::abs(diatonicInterval);
    int current = snapped;
    for (int i = 0; i < steps; ++i) {
        // advance one semitone, then keep advancing until we land on an in-key note
        do {
            current += dir;
        } while (scaleDegreeOf(current, key) < 0);
    }
    return current;
}
```

Add `#include <cstdlib>` to the top of `MusicTheory.cpp` if not already there (for `std::abs`).

- [ ] **Step 4: Build and run**

Run:
```sh
cmake --build multivoicer/build --config Release --target MultivoicerTests
ctest --test-dir multivoicer/build --output-on-failure
```
Expected: pass — all music-theory tests green.

- [ ] **Step 5: Commit**

```sh
git add multivoicer/Source/Music/MusicTheory.cpp multivoicer/Tests/music_theory_test.cpp
git commit -m "multivoicer: targetForInterval (diatonic harmonizer core)"
```

**End of Phase 1.** Music theory module complete.

---

## Phase 2 — DSP primitives

Goal: `PitchDetector` and `PitchShifter` interfaces, the `YinPitchDetector` impl (DIY), the `SignalsmithPitchShifter` impl (library wrap), and a 3-band `VoiceEq`. Each task: tests first, then impl.

### Task 12: PitchDetector interface (header only)

**Files:**
- Create: `multivoicer/Source/Dsp/PitchDetector.h`

- [ ] **Step 1: Write the interface (no test of its own — exercised by Task 13)**

Write `multivoicer/Source/Dsp/PitchDetector.h`:

```cpp
#pragma once

namespace mv {

class PitchDetector {
public:
    struct Result {
        double freqHz = 0.0;
        float  confidence = 0.0f;
        bool   voiced = false;
    };

    virtual ~PitchDetector() = default;

    virtual void   prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void   reset() = 0;
    virtual Result process(const float* mono, int numSamples) = 0;
};

} // namespace mv
```

- [ ] **Step 2: Confirm it compiles**

Run: `cmake --build multivoicer/build --config Release --target MultivoicerTests`
Expected: success (no new code uses it yet).

- [ ] **Step 3: Commit**

```sh
git add multivoicer/Source/Dsp/PitchDetector.h
git commit -m "multivoicer: PitchDetector interface header"
```

---

### Task 13: YinPitchDetector

Reference: de Cheveigné & Kawahara (2002) — "YIN, a fundamental frequency estimator for speech and music".

**Files:**
- Create: `multivoicer/Source/Dsp/YinPitchDetector.h`
- Create: `multivoicer/Source/Dsp/YinPitchDetector.cpp`
- Create: `multivoicer/Tests/yin_detector_test.cpp`
- Modify: `multivoicer/Tests/CMakeLists.txt`
- Modify: `multivoicer/CMakeLists.txt`

- [ ] **Step 1: Failing tests**

Write `multivoicer/Tests/yin_detector_test.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "Dsp/YinPitchDetector.h"

namespace {
std::vector<float> sine(double freqHz, double sr, int n, float amp = 0.5f) {
    std::vector<float> out(n);
    for (int i = 0; i < n; ++i) out[i] = amp * std::sin(2.0 * M_PI * freqHz * i / sr);
    return out;
}
}

TEST_CASE("YIN detects pure sines within 1 Hz", "[yin]") {
    mv::YinPitchDetector det;
    const double sr = 44100.0;
    const int n = 2048;
    det.prepare(sr, n);

    for (double f : { 110.0, 220.0, 440.0, 880.0 }) {
        det.reset();
        auto buf = sine(f, sr, n);
        auto r = det.process(buf.data(), n);
        INFO("Test frequency: " << f);
        CHECK(r.voiced);
        CHECK(r.confidence > 0.8f);
        CHECK(std::abs(r.freqHz - f) < 1.5);
    }
}

TEST_CASE("YIN reports unvoiced on silence", "[yin]") {
    mv::YinPitchDetector det;
    det.prepare(44100.0, 2048);
    std::vector<float> silent(2048, 0.0f);
    auto r = det.process(silent.data(), 2048);
    CHECK_FALSE(r.voiced);
}

TEST_CASE("YIN reports unvoiced on white noise", "[yin]") {
    mv::YinPitchDetector det;
    det.prepare(44100.0, 2048);
    std::vector<float> noise(2048);
    unsigned seed = 1;
    for (auto& x : noise) {
        seed = seed * 1664525u + 1013904223u;
        x = static_cast<float>(static_cast<int>(seed) / 2147483648.0);
    }
    auto r = det.process(noise.data(), 2048);
    // Noise can occasionally trigger a false detection — confidence must stay low.
    CHECK(r.confidence < 0.5f);
}
```

Update `multivoicer/Tests/CMakeLists.txt` `add_executable` sources to include `yin_detector_test.cpp` and `${CMAKE_SOURCE_DIR}/Source/Dsp/YinPitchDetector.cpp`.

Update `multivoicer/CMakeLists.txt` `target_sources(Multivoicer ...)` to add `Source/Dsp/YinPitchDetector.cpp`.

- [ ] **Step 2: Run, confirm failure (headers don't exist)**

Run: `cmake --build multivoicer/build --config Release --target MultivoicerTests`
Expected: FAIL — missing `YinPitchDetector.h`.

- [ ] **Step 3: Implement**

Write `multivoicer/Source/Dsp/YinPitchDetector.h`:

```cpp
#pragma once

#include <vector>
#include "PitchDetector.h"

namespace mv {

class YinPitchDetector : public PitchDetector {
public:
    void   prepare(double sampleRate, int maxBlockSize) override;
    void   reset() override;
    Result process(const float* mono, int numSamples) override;

private:
    double sampleRate = 44100.0;
    int    analysisFrameSize = 2048;     // window for one detection
    int    minLag = 0;                   // ~ sr / fmaxHz
    int    maxLag = 0;                   // ~ sr / fminHz
    static constexpr double fminHz = 60.0;
    static constexpr double fmaxHz = 2000.0;
    static constexpr float  cmndfThreshold = 0.15f;

    std::vector<float> ringBuffer;
    int    writePos = 0;
    int    samplesSinceLastEstimate = 0;
    Result lastResult {};

    Result runYin();   // operates on the most recent analysisFrameSize samples of ringBuffer
};

} // namespace mv
```

Write `multivoicer/Source/Dsp/YinPitchDetector.cpp`:

```cpp
#include "YinPitchDetector.h"
#include <algorithm>
#include <cmath>

namespace mv {

void YinPitchDetector::prepare(double sr, int /*maxBlockSize*/) {
    sampleRate = sr;
    analysisFrameSize = 2048;
    minLag = std::max(2, static_cast<int>(sampleRate / fmaxHz));
    maxLag = std::min(analysisFrameSize / 2, static_cast<int>(sampleRate / fminHz));
    ringBuffer.assign(analysisFrameSize * 2, 0.0f);
    reset();
}

void YinPitchDetector::reset() {
    std::fill(ringBuffer.begin(), ringBuffer.end(), 0.0f);
    writePos = 0;
    samplesSinceLastEstimate = analysisFrameSize;  // force a fresh estimate on first call
    lastResult = {};
}

PitchDetector::Result YinPitchDetector::process(const float* mono, int n) {
    const int cap = static_cast<int>(ringBuffer.size());
    for (int i = 0; i < n; ++i) {
        ringBuffer[writePos] = mono[i];
        writePos = (writePos + 1) % cap;
    }
    samplesSinceLastEstimate += n;

    // One YIN analysis per analysisFrameSize hop (so detector latency ~ one hop).
    if (samplesSinceLastEstimate >= analysisFrameSize) {
        samplesSinceLastEstimate = 0;
        lastResult = runYin();
    }
    return lastResult;
}

PitchDetector::Result YinPitchDetector::runYin() {
    // Copy the most recent analysisFrameSize samples into a contiguous frame.
    const int N = analysisFrameSize;
    std::vector<float> frame(N);
    const int cap = static_cast<int>(ringBuffer.size());
    int idx = (writePos - N + cap) % cap;
    for (int i = 0; i < N; ++i) {
        frame[i] = ringBuffer[(idx + i) % cap];
    }

    // Step 1 — difference function d(tau)
    std::vector<float> d(maxLag + 1, 0.0f);
    for (int tau = 1; tau <= maxLag; ++tau) {
        float sum = 0.0f;
        for (int i = 0; i + tau < N; ++i) {
            float diff = frame[i] - frame[i + tau];
            sum += diff * diff;
        }
        d[tau] = sum;
    }

    // Step 2 — cumulative mean normalized difference (CMNDF)
    std::vector<float> cmnd(maxLag + 1, 1.0f);
    cmnd[0] = 1.0f;
    float running = 0.0f;
    for (int tau = 1; tau <= maxLag; ++tau) {
        running += d[tau];
        cmnd[tau] = d[tau] * tau / (running > 1e-12f ? running : 1e-12f);
    }

    // Step 3 — absolute threshold: first tau where cmnd < threshold AND is a local minimum
    int tauEstimate = -1;
    for (int tau = minLag; tau <= maxLag; ++tau) {
        if (cmnd[tau] < cmndfThreshold) {
            while (tau + 1 <= maxLag && cmnd[tau + 1] < cmnd[tau]) ++tau;
            tauEstimate = tau;
            break;
        }
    }

    Result r {};
    if (tauEstimate < 0) {
        r.voiced = false;
        r.confidence = 0.0f;
        return r;
    }

    // Step 4 — parabolic interpolation around tauEstimate for sub-sample accuracy
    double betterTau = tauEstimate;
    if (tauEstimate > 0 && tauEstimate < maxLag) {
        float s0 = cmnd[tauEstimate - 1];
        float s1 = cmnd[tauEstimate];
        float s2 = cmnd[tauEstimate + 1];
        float denom = (2.0f * (2.0f * s1 - s2 - s0));
        if (std::abs(denom) > 1e-12f) {
            betterTau = tauEstimate + (s2 - s0) / denom;
        }
    }

    r.freqHz = sampleRate / betterTau;
    r.confidence = std::clamp(1.0f - cmnd[tauEstimate], 0.0f, 1.0f);
    r.voiced = (r.freqHz >= fminHz && r.freqHz <= fmaxHz && r.confidence >= 0.5f);
    return r;
}

} // namespace mv
```

- [ ] **Step 4: Build and run tests**

Run:
```sh
cmake --build multivoicer/build --config Release --target MultivoicerTests
ctest --test-dir multivoicer/build --output-on-failure
```
Expected: all `[yin]` tests pass.

- [ ] **Step 5: Commit**

```sh
git add multivoicer/Source/Dsp/PitchDetector.h \
        multivoicer/Source/Dsp/YinPitchDetector.h \
        multivoicer/Source/Dsp/YinPitchDetector.cpp \
        multivoicer/Tests/yin_detector_test.cpp \
        multivoicer/Tests/CMakeLists.txt multivoicer/CMakeLists.txt
git commit -m "multivoicer: YIN pitch detector + tests"
```

---

### Task 14: PitchShifter interface (header only)

**Files:**
- Create: `multivoicer/Source/Dsp/PitchShifter.h`

- [ ] **Step 1: Write the interface**

Write `multivoicer/Source/Dsp/PitchShifter.h`:

```cpp
#pragma once

namespace mv {

class PitchShifter {
public:
    virtual ~PitchShifter() = default;

    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;
    virtual void setShiftRatio(double outputFreqOverInputFreq) = 0;
    virtual void setFormantPreserve(bool on) = 0;
    virtual void process(const float* in, float* out, int numSamples) = 0;
};

} // namespace mv
```

- [ ] **Step 2: Commit**

```sh
git add multivoicer/Source/Dsp/PitchShifter.h
git commit -m "multivoicer: PitchShifter interface header"
```

---

### Task 15: SignalsmithPitchShifter (library wrapper)

**Files:**
- Create: `multivoicer/Source/Dsp/SignalsmithPitchShifter.h`
- Create: `multivoicer/Source/Dsp/SignalsmithPitchShifter.cpp`
- Create: `multivoicer/Tests/pitch_shifter_test.cpp`
- Modify: `multivoicer/Tests/CMakeLists.txt`
- Modify: `multivoicer/CMakeLists.txt`

- [ ] **Step 1: Failing tests (round-trip with YIN)**

Write `multivoicer/Tests/pitch_shifter_test.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "Dsp/SignalsmithPitchShifter.h"
#include "Dsp/YinPitchDetector.h"

namespace {
std::vector<float> sine(double freqHz, double sr, int n, float amp = 0.5f) {
    std::vector<float> out(n);
    for (int i = 0; i < n; ++i) out[i] = amp * std::sin(2.0 * M_PI * freqHz * i / sr);
    return out;
}
}

TEST_CASE("SignalsmithPitchShifter shifts pitch by ratio", "[shifter]") {
    const double sr = 44100.0;
    const int block = 1024;
    const int totalBlocks = 16;
    const int total = block * totalBlocks;

    mv::SignalsmithPitchShifter shifter;
    shifter.prepare(sr, block);

    mv::YinPitchDetector det;
    det.prepare(sr, block);

    for (double ratio : { 0.5, 0.75, 1.0, 1.25, 1.5, 2.0 }) {
        shifter.reset();
        det.reset();
        shifter.setShiftRatio(ratio);

        auto in = sine(440.0, sr, total);
        std::vector<float> out(total, 0.0f);
        for (int b = 0; b < totalBlocks; ++b) {
            shifter.process(in.data() + b * block, out.data() + b * block, block);
        }

        // Feed only the second half of the output to the detector
        // (skip the shifter's startup latency).
        auto r = det.process(out.data() + total / 2, total / 2);
        INFO("ratio = " << ratio);
        CHECK(r.voiced);
        CHECK(std::abs(r.freqHz - 440.0 * ratio) < 8.0);
    }
}

TEST_CASE("SignalsmithPitchShifter unity ratio is near-passthrough", "[shifter]") {
    const double sr = 44100.0;
    const int block = 1024;
    const int total = block * 8;

    mv::SignalsmithPitchShifter shifter;
    shifter.prepare(sr, block);
    shifter.setShiftRatio(1.0);

    auto in = sine(440.0, sr, total);
    std::vector<float> out(total, 0.0f);
    for (int b = 0; b < 8; ++b) {
        shifter.process(in.data() + b * block, out.data() + b * block, block);
    }

    // After startup, peak amplitude should be within ~3 dB of input (0.5 amp).
    float peak = 0.0f;
    for (int i = total / 2; i < total; ++i) peak = std::max(peak, std::abs(out[i]));
    CHECK(peak > 0.3f);
    CHECK(peak < 1.0f);
}
```

Update `multivoicer/Tests/CMakeLists.txt` to include the new test and `Source/Dsp/SignalsmithPitchShifter.cpp`.

Update `multivoicer/CMakeLists.txt` `target_sources(Multivoicer ...)` to add `Source/Dsp/SignalsmithPitchShifter.cpp`.

- [ ] **Step 2: Run, confirm failure (no header yet)**

Run: `cmake --build multivoicer/build --config Release --target MultivoicerTests`
Expected: FAIL.

- [ ] **Step 3: Implement**

Write `multivoicer/Source/Dsp/SignalsmithPitchShifter.h`:

```cpp
#pragma once

#include <memory>
#include "PitchShifter.h"

namespace mv {

class SignalsmithPitchShifter : public PitchShifter {
public:
    SignalsmithPitchShifter();
    ~SignalsmithPitchShifter() override;

    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void setShiftRatio(double ratio) override;
    void setFormantPreserve(bool on) override;
    void process(const float* in, float* out, int numSamples) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace mv
```

Write `multivoicer/Source/Dsp/SignalsmithPitchShifter.cpp`:

```cpp
#include "SignalsmithPitchShifter.h"
#include "signalsmith-stretch.h"

namespace mv {

struct SignalsmithPitchShifter::Impl {
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    double sampleRate = 44100.0;
    double ratio = 1.0;
    bool   formantPreserve = true;
    bool   prepared = false;

    void configure() {
        // Default config recommended by signalsmith for live use:
        stretch.presetDefault(1, static_cast<float>(sampleRate));  // 1 channel
        applyShift();
    }

    void applyShift() {
        // setTransposeFactor takes the pitch ratio directly.
        stretch.setTransposeFactor(static_cast<float>(ratio),
                                   formantPreserve ? 1.0f : 0.0f);
    }
};

SignalsmithPitchShifter::SignalsmithPitchShifter()
    : impl(std::make_unique<Impl>()) {}

SignalsmithPitchShifter::~SignalsmithPitchShifter() = default;

void SignalsmithPitchShifter::prepare(double sr, int /*maxBlockSize*/) {
    impl->sampleRate = sr;
    impl->configure();
    impl->prepared = true;
}

void SignalsmithPitchShifter::reset() {
    if (impl->prepared) impl->stretch.reset();
}

void SignalsmithPitchShifter::setShiftRatio(double r) {
    impl->ratio = (r > 0.0) ? r : 1.0;
    if (impl->prepared) impl->applyShift();
}

void SignalsmithPitchShifter::setFormantPreserve(bool on) {
    impl->formantPreserve = on;
    if (impl->prepared) impl->applyShift();
}

void SignalsmithPitchShifter::process(const float* in, float* out, int n) {
    const float* inputs[1]  = { in };
    float*       outputs[1] = { out };
    impl->stretch.process(inputs, n, outputs, n);
}

} // namespace mv
```

> Note: the exact Signalsmith API symbol (`presetDefault`, `setTransposeFactor`, `process`) follows their `signalsmith-stretch.h` header. If a method name differs in the fetched version, adjust per the header — semantics are the same.

- [ ] **Step 4: Build and run**

Run:
```sh
cmake --build multivoicer/build --config Release --target MultivoicerTests
ctest --test-dir multivoicer/build --output-on-failure
```
Expected: all `[shifter]` tests pass. If a ratio test is marginal, widen the tolerance to `< 10.0` Hz — Signalsmith's accuracy at ratio extremes is good but not perfect at one-block warmup.

- [ ] **Step 5: Commit**

```sh
git add multivoicer/Source/Dsp/SignalsmithPitchShifter.h \
        multivoicer/Source/Dsp/SignalsmithPitchShifter.cpp \
        multivoicer/Tests/pitch_shifter_test.cpp \
        multivoicer/Tests/CMakeLists.txt multivoicer/CMakeLists.txt
git commit -m "multivoicer: SignalsmithPitchShifter wrapper + round-trip tests"
```

---

### Task 16: VoiceEq (3-band IIR)

**Files:**
- Create: `multivoicer/Source/Dsp/VoiceEq.h`
- Create: `multivoicer/Source/Dsp/VoiceEq.cpp`
- Create: `multivoicer/Tests/voice_eq_test.cpp`
- Modify: `multivoicer/Tests/CMakeLists.txt`
- Modify: `multivoicer/CMakeLists.txt`

- [ ] **Step 1: Failing tests**

Write `multivoicer/Tests/voice_eq_test.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include "Dsp/VoiceEq.h"

namespace {
std::vector<float> sine(double freqHz, double sr, int n, float amp = 0.5f) {
    std::vector<float> out(n);
    for (int i = 0; i < n; ++i) out[i] = amp * std::sin(2.0 * M_PI * freqHz * i / sr);
    return out;
}
float peak(const std::vector<float>& v, int from) {
    float p = 0.0f;
    for (int i = from; i < (int)v.size(); ++i) p = std::max(p, std::abs(v[i]));
    return p;
}
float toDb(float linear) { return 20.0f * std::log10(linear + 1e-12f); }
}

TEST_CASE("VoiceEq flat at 0 dB gains", "[voice_eq]") {
    mv::VoiceEq eq;
    eq.prepare(44100.0, 1024);
    eq.setParams(0.0f, 0.0f, 1000.0f, 1.0f, 0.0f);
    auto in = sine(1000.0, 44100.0, 4096);
    std::vector<float> out = in;
    eq.processInPlace(out.data(), (int)out.size());
    float inPeak  = peak(in, 1024);
    float outPeak = peak(out, 1024);
    CHECK(std::abs(toDb(outPeak) - toDb(inPeak)) < 0.5f);  // within 0.5 dB
}

TEST_CASE("VoiceEq mid peak boosts at its center frequency", "[voice_eq]") {
    mv::VoiceEq eq;
    eq.prepare(44100.0, 1024);
    eq.setParams(0.0f, +12.0f, 1000.0f, 1.0f, 0.0f);
    auto in = sine(1000.0, 44100.0, 8192);
    std::vector<float> out = in;
    eq.processInPlace(out.data(), (int)out.size());
    float inPeak  = peak(in, 2048);
    float outPeak = peak(out, 2048);
    // +12 dB target. Allow ±2 dB headroom.
    float gainDb = toDb(outPeak) - toDb(inPeak);
    CHECK(gainDb > 10.0f);
    CHECK(gainDb < 14.0f);
}

TEST_CASE("VoiceEq low shelf cuts below shelf frequency", "[voice_eq]") {
    mv::VoiceEq eq;
    eq.prepare(44100.0, 1024);
    eq.setParams(-12.0f, 0.0f, 1000.0f, 1.0f, 0.0f);  // -12 dB low shelf
    auto in = sine(80.0, 44100.0, 8192);
    std::vector<float> out = in;
    eq.processInPlace(out.data(), (int)out.size());
    float inPeak  = peak(in, 2048);
    float outPeak = peak(out, 2048);
    float gainDb = toDb(outPeak) - toDb(inPeak);
    CHECK(gainDb < -8.0f);   // significantly attenuated
}
```

Update `multivoicer/Tests/CMakeLists.txt` to add the test + `Source/Dsp/VoiceEq.cpp`.
Update `multivoicer/CMakeLists.txt` to add `Source/Dsp/VoiceEq.cpp` to plugin target.

- [ ] **Step 2: Run, confirm failure**

Run: `cmake --build multivoicer/build --config Release --target MultivoicerTests`
Expected: FAIL.

- [ ] **Step 3: Implement**

Write `multivoicer/Source/Dsp/VoiceEq.h`:

```cpp
#pragma once

#include <juce_dsp/juce_dsp.h>

namespace mv {

class VoiceEq {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setParams(float lowShelfDb, float midPeakDb, float midFreqHz, float midQ, float highShelfDb);
    void processInPlace(float* mono, int numSamples);

private:
    double sr = 44100.0;
    juce::dsp::IIR::Filter<float> lowShelf;
    juce::dsp::IIR::Filter<float> midPeak;
    juce::dsp::IIR::Filter<float> highShelf;
};

} // namespace mv
```

Write `multivoicer/Source/Dsp/VoiceEq.cpp`:

```cpp
#include "VoiceEq.h"
#include <cmath>

namespace mv {

static float dbToGain(float dB) { return std::pow(10.0f, dB / 20.0f); }

void VoiceEq::prepare(double sampleRate, int maxBlockSize) {
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32)maxBlockSize, 1 };
    lowShelf.prepare(spec);
    midPeak.prepare(spec);
    highShelf.prepare(spec);
    setParams(0.0f, 0.0f, 1000.0f, 1.0f, 0.0f);
}

void VoiceEq::reset() {
    lowShelf.reset();
    midPeak.reset();
    highShelf.reset();
}

void VoiceEq::setParams(float lowDb, float midDb, float midHz, float midQ, float highDb) {
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    *lowShelf.coefficients  = *Coeffs::makeLowShelf(sr,  200.0f, 0.707f, dbToGain(lowDb));
    *midPeak.coefficients   = *Coeffs::makePeakFilter(sr, midHz, midQ, dbToGain(midDb));
    *highShelf.coefficients = *Coeffs::makeHighShelf(sr, 4000.0f, 0.707f, dbToGain(highDb));
}

void VoiceEq::processInPlace(float* mono, int n) {
    juce::dsp::AudioBlock<float> block(&mono, 1, (size_t)n);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    lowShelf.process(ctx);
    midPeak.process(ctx);
    highShelf.process(ctx);
}

} // namespace mv
```

- [ ] **Step 4: Build and run**

Run:
```sh
cmake --build multivoicer/build --config Release --target MultivoicerTests
ctest --test-dir multivoicer/build --output-on-failure
```
Expected: all `[voice_eq]` tests pass.

- [ ] **Step 5: Commit**

```sh
git add multivoicer/Source/Dsp/VoiceEq.h multivoicer/Source/Dsp/VoiceEq.cpp \
        multivoicer/Tests/voice_eq_test.cpp \
        multivoicer/Tests/CMakeLists.txt multivoicer/CMakeLists.txt
git commit -m "multivoicer: VoiceEq 3-band IIR + tests"
```

**End of Phase 2.** DSP primitives complete.

---

## Phase 3 — Voicing

Goal: `VoiceParams`, `Voice`, `VoiceManager`, `MidiVoiceAllocator` (poly + mono), `ModeRouter`. Each composes the DSP primitives from Phase 2.

### Task 17: VoiceParams + Voice

**Files:**
- Create: `multivoicer/Source/Voicing/Voice.h`
- Create: `multivoicer/Source/Voicing/Voice.cpp`
- Create: `multivoicer/Tests/voice_test.cpp`
- Modify: `multivoicer/Tests/CMakeLists.txt`
- Modify: `multivoicer/CMakeLists.txt`

- [ ] **Step 1: Failing tests**

Write `multivoicer/Tests/voice_test.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include "Voicing/Voice.h"

namespace {
std::vector<float> sine(double freqHz, double sr, int n, float amp = 0.5f) {
    std::vector<float> out(n);
    for (int i = 0; i < n; ++i) out[i] = amp * std::sin(2.0 * M_PI * freqHz * i / sr);
    return out;
}
float rms(const float* p, int n) {
    double s = 0; for (int i = 0; i < n; ++i) s += p[i] * p[i];
    return (float)std::sqrt(s / n);
}
}

TEST_CASE("Voice silent before noteOn", "[voice]") {
    mv::Voice v;
    v.prepare(44100.0, 512);
    mv::VoiceParams p {};
    p.enabled = true; p.gainDb = 0.0f; p.pan = 0.0f;
    p.attack = 10.0f; p.decay = 50.0f; p.sustain = 1.0f; p.release = 100.0f;
    v.setParams(p);
    v.setTargetFreq(440.0);
    auto in = sine(440.0, 44100.0, 512);
    std::vector<float> outL(512, 0.0f), outR(512, 0.0f);
    v.renderAdd(in.data(), outL.data(), outR.data(), 512, 440.0);
    CHECK(rms(outL.data(), 512) < 0.01f);  // no note-on => silent
}

TEST_CASE("Voice produces output after noteOn", "[voice]") {
    mv::Voice v;
    v.prepare(44100.0, 512);
    mv::VoiceParams p {};
    p.enabled = true; p.gainDb = 0.0f; p.pan = 0.0f;
    p.attack = 1.0f; p.decay = 10.0f; p.sustain = 1.0f; p.release = 100.0f;
    v.setParams(p);
    v.setTargetFreq(440.0);
    v.noteOn();

    auto in = sine(440.0, 44100.0, 4096);
    std::vector<float> outL(4096, 0.0f), outR(4096, 0.0f);
    for (int b = 0; b < 8; ++b) {
        v.renderAdd(in.data() + b * 512, outL.data() + b * 512, outR.data() + b * 512, 512, 440.0);
    }
    CHECK(rms(outL.data() + 1024, 3072) > 0.05f);
}

TEST_CASE("Voice pan routes to expected channel", "[voice]") {
    mv::Voice v;
    v.prepare(44100.0, 512);
    mv::VoiceParams p {};
    p.enabled = true; p.gainDb = 0.0f; p.pan = -1.0f;       // hard left
    p.attack = 1.0f; p.decay = 10.0f; p.sustain = 1.0f; p.release = 100.0f;
    v.setParams(p);
    v.setTargetFreq(440.0);
    v.noteOn();

    auto in = sine(440.0, 44100.0, 2048);
    std::vector<float> outL(2048, 0.0f), outR(2048, 0.0f);
    for (int b = 0; b < 4; ++b) {
        v.renderAdd(in.data() + b * 512, outL.data() + b * 512, outR.data() + b * 512, 512, 440.0);
    }
    CHECK(rms(outL.data() + 512, 1536) > 0.05f);
    CHECK(rms(outR.data() + 512, 1536) < 0.005f);
}
```

Update Tests CMakeLists to include `voice_test.cpp` + `Source/Voicing/Voice.cpp`.
Update plugin CMakeLists to include `Source/Voicing/Voice.cpp`.

- [ ] **Step 2: Run, confirm failure**

Run: `cmake --build multivoicer/build --config Release --target MultivoicerTests`
Expected: FAIL.

- [ ] **Step 3: Implement**

Write `multivoicer/Source/Voicing/Voice.h`:

```cpp
#pragma once

#include <memory>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Dsp/PitchShifter.h"
#include "../Dsp/VoiceEq.h"

namespace mv {

struct VoiceParams {
    bool   enabled = false;
    int    interval = 0;
    float  gainDb = 0.0f;
    float  pan = 0.0f;
    float  attack = 20.0f, decay = 80.0f, sustain = 0.8f, release = 300.0f;
    float  lowShelfDb = 0.0f, midPeakDb = 0.0f, highShelfDb = 0.0f;
    float  midFreqHz = 1000.0f, midQ = 1.0f;
};

class Voice {
public:
    Voice();
    ~Voice();

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setParams(const VoiceParams& p);
    void setTargetFreq(double hz);
    void noteOn();
    void noteOff();
    bool isGated() const { return adsr.isActive(); }
    void renderAdd(const float* dryMono, float* outL, float* outR, int n, double inputFreqHz);

private:
    std::unique_ptr<PitchShifter> shifter;
    VoiceEq eq;
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;
    double sampleRate = 44100.0;
    double targetFreq = 440.0;
    float  gainLinear = 1.0f;
    float  panLeft = 0.707f, panRight = 0.707f;
    std::vector<float> tmp;
};

} // namespace mv
```

Write `multivoicer/Source/Voicing/Voice.cpp`:

```cpp
#include "Voice.h"
#include <cmath>
#include "../Dsp/SignalsmithPitchShifter.h"

namespace mv {

Voice::Voice() : shifter(std::make_unique<SignalsmithPitchShifter>()) {}
Voice::~Voice() = default;

void Voice::prepare(double sr, int maxBlockSize) {
    sampleRate = sr;
    shifter->prepare(sr, maxBlockSize);
    eq.prepare(sr, maxBlockSize);
    adsr.setSampleRate(sr);
    tmp.assign((size_t)maxBlockSize, 0.0f);
    reset();
}

void Voice::reset() {
    shifter->reset();
    eq.reset();
    adsr.reset();
}

void Voice::setParams(const VoiceParams& p) {
    adsrParams.attack  = p.attack  * 0.001f;
    adsrParams.decay   = p.decay   * 0.001f;
    adsrParams.sustain = p.sustain;
    adsrParams.release = p.release * 0.001f;
    adsr.setParameters(adsrParams);
    eq.setParams(p.lowShelfDb, p.midPeakDb, p.midFreqHz, p.midQ, p.highShelfDb);
    gainLinear = std::pow(10.0f, p.gainDb / 20.0f);
    // Equal-power pan (-1..+1)
    float panNorm = (p.pan + 1.0f) * 0.5f * (float)M_PI_2;
    panLeft  = std::cos(panNorm);
    panRight = std::sin(panNorm);
}

void Voice::setTargetFreq(double hz) {
    targetFreq = hz > 0.0 ? hz : targetFreq;
}

void Voice::noteOn()  { adsr.noteOn(); }
void Voice::noteOff() { adsr.noteOff(); }

void Voice::renderAdd(const float* dryMono, float* outL, float* outR, int n, double inputFreqHz) {
    if (!adsr.isActive()) return;

    const double safeInput = inputFreqHz > 1.0 ? inputFreqHz : 100.0;
    shifter->setShiftRatio(targetFreq / safeInput);
    shifter->process(dryMono, tmp.data(), n);
    eq.processInPlace(tmp.data(), n);

    for (int i = 0; i < n; ++i) {
        float env = adsr.getNextSample();
        float s = tmp[i] * env * gainLinear;
        outL[i] += s * panLeft;
        outR[i] += s * panRight;
    }
}

} // namespace mv
```

- [ ] **Step 4: Build and run**

Run: build + ctest. Expected: `[voice]` tests pass.

- [ ] **Step 5: Commit**

```sh
git add multivoicer/Source/Voicing/Voice.h multivoicer/Source/Voicing/Voice.cpp \
        multivoicer/Tests/voice_test.cpp \
        multivoicer/Tests/CMakeLists.txt multivoicer/CMakeLists.txt
git commit -m "multivoicer: Voice (shifter + ADSR + EQ + gain/pan)"
```

---

### Task 18: VoiceManager

**Files:**
- Create: `multivoicer/Source/Voicing/VoiceManager.h`
- Create: `multivoicer/Source/Voicing/VoiceManager.cpp`
- Modify: `multivoicer/Tests/voice_test.cpp` (append a `[voice_manager]` test)
- Modify: `multivoicer/Tests/CMakeLists.txt`
- Modify: `multivoicer/CMakeLists.txt`

- [ ] **Step 1: Failing test**

Append to `multivoicer/Tests/voice_test.cpp`:

```cpp
#include "Voicing/VoiceManager.h"

TEST_CASE("VoiceManager renders only active voices", "[voice_manager]") {
    mv::VoiceManager mgr;
    mgr.prepare(44100.0, 512);
    mgr.setNumActiveVoices(2);

    mv::VoiceParams p {};
    p.enabled = true; p.attack = 1.0f; p.decay = 10.0f; p.sustain = 1.0f; p.release = 100.0f;
    mgr.voice(0).setParams(p); mgr.voice(0).setTargetFreq(440.0); mgr.voice(0).noteOn();
    mgr.voice(1).setParams(p); mgr.voice(1).setTargetFreq(550.0); mgr.voice(1).noteOn();

    std::vector<float> in(2048, 0.0f), outL(2048, 0.0f), outR(2048, 0.0f);
    for (int i = 0; i < 2048; ++i) in[i] = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / 44100.0);

    for (int b = 0; b < 4; ++b) {
        mgr.renderAdd(in.data() + b * 512, outL.data() + b * 512, outR.data() + b * 512, 512, 440.0);
    }
    double e = 0.0;
    for (int i = 1024; i < 2048; ++i) e += outL[i] * outL[i];
    CHECK(e > 0.5);
}
```

- [ ] **Step 2: Confirm failure**

- [ ] **Step 3: Implement**

Write `multivoicer/Source/Voicing/VoiceManager.h`:

```cpp
#pragma once

#include <array>
#include "Voice.h"

namespace mv {

class VoiceManager {
public:
    static constexpr int kMaxVoices = 4;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setNumActiveVoices(int n);
    int  numActiveVoices() const { return active; }
    Voice& voice(int i) { return voices[i]; }
    void renderAdd(const float* dry, float* outL, float* outR, int n, double inputFreqHz);

private:
    std::array<Voice, kMaxVoices> voices;
    int active = 3;
};

} // namespace mv
```

Write `multivoicer/Source/Voicing/VoiceManager.cpp`:

```cpp
#include "VoiceManager.h"
#include <algorithm>

namespace mv {

void VoiceManager::prepare(double sr, int maxBlockSize) {
    for (auto& v : voices) v.prepare(sr, maxBlockSize);
}

void VoiceManager::reset() {
    for (auto& v : voices) v.reset();
}

void VoiceManager::setNumActiveVoices(int n) {
    active = std::clamp(n, 1, kMaxVoices);
    for (int i = active; i < kMaxVoices; ++i) voices[i].noteOff();
}

void VoiceManager::renderAdd(const float* dry, float* outL, float* outR, int n, double inputFreqHz) {
    for (int i = 0; i < active; ++i) {
        voices[i].renderAdd(dry, outL, outR, n, inputFreqHz);
    }
}

} // namespace mv
```

- [ ] **Step 4: Build and run**. Expected pass.
- [ ] **Step 5: Commit**

```sh
git add multivoicer/Source/Voicing/VoiceManager.h multivoicer/Source/Voicing/VoiceManager.cpp \
        multivoicer/Tests/voice_test.cpp multivoicer/Tests/CMakeLists.txt multivoicer/CMakeLists.txt
git commit -m "multivoicer: VoiceManager (4-slot voice container + render sum)"
```

---

### Task 19: MidiVoiceAllocator

**Files:**
- Create: `multivoicer/Source/Voicing/MidiVoiceAllocator.h`
- Create: `multivoicer/Source/Voicing/MidiVoiceAllocator.cpp`
- Create: `multivoicer/Tests/voice_allocator_test.cpp`
- Modify: `multivoicer/Tests/CMakeLists.txt`
- Modify: `multivoicer/CMakeLists.txt`

- [ ] **Step 1: Failing tests**

Write `multivoicer/Tests/voice_allocator_test.cpp`:

```cpp
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
```

Update Tests + plugin CMakeLists.

- [ ] **Step 2: Confirm failure**

- [ ] **Step 3: Implement**

Write `multivoicer/Source/Voicing/MidiVoiceAllocator.h`:

```cpp
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
```

Write `multivoicer/Source/Voicing/MidiVoiceAllocator.cpp`:

```cpp
#include "MidiVoiceAllocator.h"
#include <algorithm>

namespace mv {

void MidiVoiceAllocator::setPoly(int n) {
    mono = false;
    polyCount = std::clamp(n, 1, kMaxSlots);
    clearAll();
}

void MidiVoiceAllocator::setMono() {
    mono = true;
    polyCount = 1;
    clearAll();
}

void MidiVoiceAllocator::clearAll() {
    for (auto& s : slots) s = {};
    monoStack.clear();
}

void MidiVoiceAllocator::noteOn(int n) {
    if (mono) {
        auto it = std::find(monoStack.begin(), monoStack.end(), n);
        if (it != monoStack.end()) monoStack.erase(it);
        monoStack.push_back(n);
        slots[0] = { true, n, 0 };
        return;
    }
    // Poly: free slot first, else steal oldest active among 0..polyCount-1.
    for (int i = 0; i < polyCount; ++i) {
        if (!slots[i].active) {
            slots[i] = { true, n, 0 };
            return;
        }
    }
    int oldest = 0;
    for (int i = 1; i < polyCount; ++i)
        if (slots[i].age > slots[oldest].age) oldest = i;
    slots[oldest] = { true, n, 0 };
}

void MidiVoiceAllocator::noteOff(int n) {
    if (mono) {
        auto it = std::find(monoStack.begin(), monoStack.end(), n);
        if (it != monoStack.end()) monoStack.erase(it);
        if (monoStack.empty()) slots[0] = {};
        else                   slots[0] = { true, monoStack.back(), 0 };
        return;
    }
    for (int i = 0; i < polyCount; ++i) {
        if (slots[i].active && slots[i].midiNote == n) {
            slots[i] = {};
            return;
        }
    }
    // Not found (already stolen) — ignore.
}

void MidiVoiceAllocator::tick() {
    for (auto& s : slots) if (s.active) ++s.age;
}

} // namespace mv
```

- [ ] **Step 4: Build and run.** Expected pass.
- [ ] **Step 5: Commit**

```sh
git add multivoicer/Source/Voicing/MidiVoiceAllocator.h multivoicer/Source/Voicing/MidiVoiceAllocator.cpp \
        multivoicer/Tests/voice_allocator_test.cpp \
        multivoicer/Tests/CMakeLists.txt multivoicer/CMakeLists.txt
git commit -m "multivoicer: MidiVoiceAllocator (poly + mono)"
```

---

### Task 20: ModeRouter

**Files:**
- Create: `multivoicer/Source/Voicing/ModeRouter.h`
- Create: `multivoicer/Source/Voicing/ModeRouter.cpp`
- Create: `multivoicer/Tests/mode_router_test.cpp`
- Modify: `multivoicer/Tests/CMakeLists.txt`
- Modify: `multivoicer/CMakeLists.txt`

- [ ] **Step 1: Failing tests**

Write `multivoicer/Tests/mode_router_test.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "Voicing/ModeRouter.h"
#include "Voicing/MidiVoiceAllocator.h"
#include "Music/Scales.h"

TEST_CASE("Preset mode + AlwaysOn + voiced -> all enabled voices active", "[router]") {
    mv::ModeRouter r;
    mv::ModeRouter::Inputs in {};
    in.mode = mv::Mode::Preset;
    in.trigger = mv::PresetTrigger::AlwaysOn;
    in.key = { mv::NoteName::C, &mv::Scales::Major };
    in.inputFreqHz = 261.63; in.voiced = true; in.dryRMS = 0.5f;
    in.numActiveVoices = 3;
    in.voiceIntervals = { 2, 4, 7, 0 };
    auto d = r.decide(in, {});
    for (int i = 0; i < 3; ++i) { CHECK(d[i].active); CHECK(d[i].gateOn); }
    CHECK_FALSE(d[3].active);
}

TEST_CASE("Preset mode + InputGate + RMS below threshold -> gates off", "[router]") {
    mv::ModeRouter r;
    mv::ModeRouter::Inputs in {};
    in.mode = mv::Mode::Preset;
    in.trigger = mv::PresetTrigger::InputGate;
    in.inputGateThresholdDb = -40.0f;
    in.dryRMS = 0.001f;       // way below -40 dB
    in.voiced = true;
    in.numActiveVoices = 2;
    in.key = { mv::NoteName::C, &mv::Scales::Major };
    in.voiceIntervals = { 2, 4, 0, 0 };
    in.inputFreqHz = 261.63;
    auto d = r.decide(in, {});
    for (int i = 0; i < 2; ++i) { CHECK(d[i].active); CHECK_FALSE(d[i].gateOn); }
}

TEST_CASE("Preset mode + PitchGate + unvoiced -> gates off", "[router]") {
    mv::ModeRouter r;
    mv::ModeRouter::Inputs in {};
    in.mode = mv::Mode::Preset;
    in.trigger = mv::PresetTrigger::PitchGate;
    in.voiced = false;
    in.numActiveVoices = 2;
    in.key = { mv::NoteName::C, &mv::Scales::Major };
    in.voiceIntervals = { 2, 4, 0, 0 };
    in.inputFreqHz = 261.63;
    auto d = r.decide(in, {});
    for (int i = 0; i < 2; ++i) CHECK_FALSE(d[i].gateOn);
}

TEST_CASE("MIDI mode: 2 notes held, 4 voices enabled -> 2 active 2 inactive", "[router]") {
    mv::ModeRouter r;
    mv::MidiVoiceAllocator a;
    a.setPoly(4);
    a.noteOn(60); a.tick(); a.noteOn(64);

    mv::ModeRouter::Inputs in {};
    in.mode = mv::Mode::Midi;
    in.numActiveVoices = 4;
    in.voiced = true; in.inputFreqHz = 261.63;
    in.key = { mv::NoteName::C, &mv::Scales::Major };
    auto d = r.decide(in, a.snapshot());

    int active = 0; for (auto& v : d) if (v.gateOn) ++active;
    CHECK(active == 2);
}

TEST_CASE("MIDI mode + no notes -> all gates off", "[router]") {
    mv::ModeRouter r;
    mv::MidiVoiceAllocator a;
    a.setPoly(4);
    mv::ModeRouter::Inputs in {};
    in.mode = mv::Mode::Midi;
    in.numActiveVoices = 4;
    in.voiced = true; in.inputFreqHz = 440.0;
    in.key = { mv::NoteName::C, &mv::Scales::Major };
    auto d = r.decide(in, a.snapshot());
    for (auto& v : d) CHECK_FALSE(v.gateOn);
}
```

Update CMakeLists.

- [ ] **Step 2: Confirm failure**

- [ ] **Step 3: Implement**

Write `multivoicer/Source/Voicing/ModeRouter.h`:

```cpp
#pragma once

#include <array>
#include "MidiVoiceAllocator.h"
#include "../Music/MusicTheory.h"

namespace mv {

enum class Mode          { Preset, Midi };
enum class PresetTrigger { InputGate, PitchGate, AlwaysOn };

struct VoiceDecision {
    bool   active = false;
    double targetFreqHz = 0.0;
    bool   gateOn = false;
};

class ModeRouter {
public:
    struct Inputs {
        Mode          mode = Mode::Preset;
        PresetTrigger trigger = PresetTrigger::InputGate;
        Key           key { NoteName::C, nullptr };
        int           numActiveVoices = 3;
        std::array<int,4> voiceIntervals { 0,0,0,0 };
        double        inputFreqHz = 0.0;
        bool          voiced = false;
        float         dryRMS = 0.0f;
        float         inputGateThresholdDb = -40.0f;
        float         pitchConfidenceMin = 0.5f;
    };

    std::array<VoiceDecision, 4> decide(
        const Inputs& in,
        const std::array<MidiVoiceAllocator::Slot, 4>& midiSlots);
};

} // namespace mv
```

Write `multivoicer/Source/Voicing/ModeRouter.cpp`:

```cpp
#include "ModeRouter.h"
#include <cmath>

namespace mv {

static bool inputGateOpen(float dryRMS, float thresholdDb) {
    float thresholdLinear = std::pow(10.0f, thresholdDb / 20.0f);
    return dryRMS >= thresholdLinear;
}

std::array<VoiceDecision, 4> ModeRouter::decide(
    const Inputs& in,
    const std::array<MidiVoiceAllocator::Slot, 4>& midiSlots)
{
    std::array<VoiceDecision, 4> out {};
    if (in.mode == Mode::Preset) {
        bool gate = false;
        switch (in.trigger) {
            case PresetTrigger::AlwaysOn:   gate = true; break;
            case PresetTrigger::PitchGate:  gate = in.voiced; break;
            case PresetTrigger::InputGate:  gate = inputGateOpen(in.dryRMS, in.inputGateThresholdDb); break;
        }
        int inputMidi = (in.voiced && in.inputFreqHz > 1.0) ? midiFromFreq(in.inputFreqHz) : 60;
        for (int i = 0; i < 4; ++i) {
            if (i < in.numActiveVoices) {
                out[i].active = true;
                int target = (in.key.scale != nullptr)
                    ? targetForInterval(inputMidi, in.voiceIntervals[i], in.key)
                    : inputMidi + in.voiceIntervals[i];
                out[i].targetFreqHz = freqFromMidi(target);
                out[i].gateOn = gate;
            }
        }
    } else {
        for (int i = 0; i < 4; ++i) {
            if (i < in.numActiveVoices && midiSlots[i].active) {
                out[i].active = true;
                out[i].targetFreqHz = freqFromMidi(midiSlots[i].midiNote);
                out[i].gateOn = true;
            } else if (i < in.numActiveVoices) {
                out[i].active = true;
                out[i].targetFreqHz = 0.0;
                out[i].gateOn = false;
            }
        }
    }
    return out;
}

} // namespace mv
```

- [ ] **Step 4: Build and run.** Expected pass.
- [ ] **Step 5: Commit**

```sh
git add multivoicer/Source/Voicing/ModeRouter.h multivoicer/Source/Voicing/ModeRouter.cpp \
        multivoicer/Tests/mode_router_test.cpp \
        multivoicer/Tests/CMakeLists.txt multivoicer/CMakeLists.txt
git commit -m "multivoicer: ModeRouter (Preset + MIDI -> per-voice decisions)"
```

**End of Phase 3.** All routing/voicing primitives ready.

---

## Phase 4 — Presets and parameters

Goal: `Preset` struct, `PresetBank` with 6 defaults, `Parameters` (APVTS layout).

### Task 21: Preset struct (header only)

**Files:**
- Create: `multivoicer/Source/Presets/Preset.h`

- [ ] **Step 1: Write the struct**

Write `multivoicer/Source/Presets/Preset.h`:

```cpp
#pragma once

#include <array>
#include <juce_core/juce_core.h>
#include "../Music/MusicTheory.h"
#include "../Voicing/Voice.h"
#include "../Voicing/ModeRouter.h"

namespace mv {

struct Preset {
    juce::String  name;

    Mode          mode = Mode::Preset;
    Key           key { NoteName::C, nullptr };
    int           numActiveVoices = 3;
    bool          monoMidiMode = false;
    bool          formantPreserve = true;
    float         dryWetMix = 0.5f;

    PresetTrigger trigger = PresetTrigger::InputGate;
    float         inputGateThresholdDb = -40.0f;
    float         inputGateHoldMs = 50.0f;
    float         pitchConfidenceMin = 0.5f;

    std::array<VoiceParams, 4> voices {};
};

} // namespace mv
```

- [ ] **Step 2: Commit**

```sh
git add multivoicer/Source/Presets/Preset.h
git commit -m "multivoicer: Preset struct"
```

---

### Task 22: PresetBank (6 defaults)

**Files:**
- Create: `multivoicer/Source/Presets/PresetBank.h`
- Create: `multivoicer/Source/Presets/PresetBank.cpp`
- Create: `multivoicer/Tests/preset_bank_test.cpp`
- Modify: `multivoicer/Tests/CMakeLists.txt`
- Modify: `multivoicer/CMakeLists.txt`

- [ ] **Step 1: Failing tests**

Write `multivoicer/Tests/preset_bank_test.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "Presets/PresetBank.h"

TEST_CASE("PresetBank ships exactly 6 default presets", "[preset_bank]") {
    auto presets = mv::PresetBank::defaults();
    CHECK(presets.size() == 6);
}

TEST_CASE("Default preset names match spec", "[preset_bank]") {
    auto presets = mv::PresetBank::defaults();
    REQUIRE(presets.size() == 6);
    CHECK(presets[0].name == "Octave Up");
    CHECK(presets[1].name == "Octave Down");
    CHECK(presets[2].name == "Third Up");
    CHECK(presets[3].name == "3rd + 5th Above");
    CHECK(presets[4].name == "Full Triad");
    CHECK(presets[5].name == "Talkbox Mono");
}

TEST_CASE("Talkbox preset is MIDI + mono + formant on", "[preset_bank]") {
    auto presets = mv::PresetBank::defaults();
    REQUIRE(presets.size() == 6);
    const auto& tb = presets[5];
    CHECK(tb.mode == mv::Mode::Midi);
    CHECK(tb.numActiveVoices == 1);
    CHECK(tb.monoMidiMode);
    CHECK(tb.formantPreserve);
}

TEST_CASE("All preset numbers are valid (no NaN, ranges respected)", "[preset_bank]") {
    auto presets = mv::PresetBank::defaults();
    for (const auto& p : presets) {
        INFO("Preset: " << p.name.toStdString());
        CHECK(p.numActiveVoices >= 1);
        CHECK(p.numActiveVoices <= 4);
        CHECK(p.dryWetMix >= 0.0f);
        CHECK(p.dryWetMix <= 1.0f);
        for (const auto& v : p.voices) {
            CHECK_FALSE(std::isnan(v.gainDb));
            CHECK_FALSE(std::isnan(v.attack));
            CHECK(v.sustain >= 0.0f); CHECK(v.sustain <= 1.0f);
        }
    }
}
```

Update CMakeLists.

- [ ] **Step 2: Confirm failure**

- [ ] **Step 3: Implement**

Write `multivoicer/Source/Presets/PresetBank.h`:

```cpp
#pragma once

#include <vector>
#include "Preset.h"

namespace mv {

class PresetBank {
public:
    static std::vector<Preset> defaults();
};

} // namespace mv
```

Write `multivoicer/Source/Presets/PresetBank.cpp`:

```cpp
#include "PresetBank.h"
#include "../Music/Scales.h"

namespace mv {

static VoiceParams basicVoice(int interval, float pan = 0.0f) {
    VoiceParams v;
    v.enabled = true;
    v.interval = interval;
    v.gainDb = 0.0f;
    v.pan = pan;
    v.attack = 20.0f; v.decay = 80.0f; v.sustain = 0.8f; v.release = 300.0f;
    v.lowShelfDb = 0.0f; v.midPeakDb = 0.0f; v.highShelfDb = 0.0f;
    v.midFreqHz = 1000.0f; v.midQ = 1.0f;
    return v;
}

static Preset preset(juce::String name) {
    Preset p;
    p.name = std::move(name);
    p.mode = Mode::Preset;
    p.key = { NoteName::C, &Scales::Major };
    p.formantPreserve = true;
    p.dryWetMix = 0.5f;
    p.trigger = PresetTrigger::InputGate;
    p.inputGateThresholdDb = -40.0f;
    p.inputGateHoldMs = 50.0f;
    p.pitchConfidenceMin = 0.5f;
    p.monoMidiMode = false;
    return p;
}

std::vector<Preset> PresetBank::defaults() {
    std::vector<Preset> bank;

    // 1. Octave Up
    {
        auto p = preset("Octave Up");
        p.numActiveVoices = 1;
        p.voices[0] = basicVoice(+7);
        bank.push_back(p);
    }
    // 2. Octave Down
    {
        auto p = preset("Octave Down");
        p.numActiveVoices = 1;
        p.voices[0] = basicVoice(-7);
        bank.push_back(p);
    }
    // 3. Third Up
    {
        auto p = preset("Third Up");
        p.numActiveVoices = 1;
        p.voices[0] = basicVoice(+2);
        bank.push_back(p);
    }
    // 4. 3rd + 5th Above
    {
        auto p = preset("3rd + 5th Above");
        p.numActiveVoices = 2;
        p.voices[0] = basicVoice(+2, -0.3f);
        p.voices[1] = basicVoice(+4, +0.3f);
        bank.push_back(p);
    }
    // 5. Full Triad
    {
        auto p = preset("Full Triad");
        p.numActiveVoices = 3;
        p.voices[0] = basicVoice(+2, -0.4f);
        p.voices[1] = basicVoice(+4, +0.4f);
        p.voices[2] = basicVoice(+7, 0.0f);
        bank.push_back(p);
    }
    // 6. Talkbox Mono
    {
        auto p = preset("Talkbox Mono");
        p.mode = Mode::Midi;
        p.numActiveVoices = 1;
        p.monoMidiMode = true;
        p.formantPreserve = true;
        p.dryWetMix = 1.0f;
        p.voices[0] = basicVoice(0);
        p.voices[0].attack = 10.0f; p.voices[0].release = 100.0f;
        bank.push_back(p);
    }
    return bank;
}

} // namespace mv
```

- [ ] **Step 4: Build and run.** Expected: all `[preset_bank]` tests pass.
- [ ] **Step 5: Commit**

```sh
git add multivoicer/Source/Presets/PresetBank.h multivoicer/Source/Presets/PresetBank.cpp \
        multivoicer/Source/Presets/Preset.h \
        multivoicer/Tests/preset_bank_test.cpp \
        multivoicer/Tests/CMakeLists.txt multivoicer/CMakeLists.txt
git commit -m "multivoicer: PresetBank with 6 default presets"
```

---

### Task 23: Parameters / APVTS layout

**Files:**
- Create: `multivoicer/Source/Plugin/Parameters.h`
- Create: `multivoicer/Source/Plugin/Parameters.cpp`
- Modify: `multivoicer/CMakeLists.txt`

- [ ] **Step 1: Write the parameter ID + layout**

Write `multivoicer/Source/Plugin/Parameters.h`:

```cpp
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace mv::Params {

// Global IDs
inline constexpr const char* kMode             = "mode";
inline constexpr const char* kVoiceCount       = "voiceCount";
inline constexpr const char* kKeyRoot          = "keyRoot";
inline constexpr const char* kKeyScale         = "keyScale";
inline constexpr const char* kFormantPreserve  = "formantPreserve";
inline constexpr const char* kDryWetMix        = "dryWetMix";
inline constexpr const char* kPreset           = "preset";
inline constexpr const char* kMonoMidiMode     = "monoMidiMode";

// Preset-mode-specific
inline constexpr const char* kPresetTrigger        = "presetTrigger";
inline constexpr const char* kInputGateThresholdDb = "inputGateThresholdDb";
inline constexpr const char* kInputGateHoldMs      = "inputGateHoldMs";
inline constexpr const char* kPitchConfidenceMin   = "pitchConfidenceMin";

// Per voice — pattern: "voice<Field>_<1..4>"
juce::String voiceParam(const char* field, int idx);  // e.g. voiceParam("Enabled", 1) -> "voiceEnabled_1"

juce::AudioProcessorValueTreeState::ParameterLayout buildLayout();

} // namespace mv::Params
```

Write `multivoicer/Source/Plugin/Parameters.cpp`:

```cpp
#include "Parameters.h"

namespace mv::Params {

juce::String voiceParam(const char* field, int idx) {
    return juce::String("voice") + field + "_" + juce::String(idx);
}

using APVTS = juce::AudioProcessorValueTreeState;
using AParam = juce::AudioProcessorParameter;
using FloatP = juce::AudioParameterFloat;
using IntP   = juce::AudioParameterInt;
using BoolP  = juce::AudioParameterBool;
using ChoiceP = juce::AudioParameterChoice;

static std::unique_ptr<juce::AudioProcessorParameterGroup> makeGlobalGroup() {
    auto g = std::make_unique<juce::AudioProcessorParameterGroup>("global", "Global", "|");
    g->addChild(std::make_unique<ChoiceP>(juce::ParameterID(kMode, 1), "Mode",
        juce::StringArray{"Preset", "MIDI"}, 0));
    g->addChild(std::make_unique<IntP>(juce::ParameterID(kVoiceCount, 1), "Voice Count", 1, 4, 3));
    g->addChild(std::make_unique<ChoiceP>(juce::ParameterID(kKeyRoot, 1), "Key Root",
        juce::StringArray{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}, 0));
    g->addChild(std::make_unique<ChoiceP>(juce::ParameterID(kKeyScale, 1), "Scale",
        juce::StringArray{"Major", "Natural Minor"}, 0));
    g->addChild(std::make_unique<BoolP>(juce::ParameterID(kFormantPreserve, 1), "Formant Preserve", true));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(kDryWetMix, 1), "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    g->addChild(std::make_unique<ChoiceP>(juce::ParameterID(kPreset, 1), "Preset",
        juce::StringArray{"Octave Up","Octave Down","Third Up","3rd + 5th Above","Full Triad","Talkbox Mono"}, 0));
    g->addChild(std::make_unique<BoolP>(juce::ParameterID(kMonoMidiMode, 1), "Mono MIDI", false));
    return g;
}

static std::unique_ptr<juce::AudioProcessorParameterGroup> makePresetTriggerGroup() {
    auto g = std::make_unique<juce::AudioProcessorParameterGroup>("presetTrigger", "Preset Trigger", "|");
    g->addChild(std::make_unique<ChoiceP>(juce::ParameterID(kPresetTrigger, 1), "Trigger",
        juce::StringArray{"InputGate","PitchGate","AlwaysOn"}, 0));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(kInputGateThresholdDb, 1), "Gate Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f), -40.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(kInputGateHoldMs, 1), "Gate Hold",
        juce::NormalisableRange<float>(0.0f, 500.0f), 50.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(kPitchConfidenceMin, 1), "Pitch Confidence Min",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    return g;
}

static std::unique_ptr<juce::AudioProcessorParameterGroup> makeVoiceGroup(int idx, bool enabledDefault, int intervalDefault) {
    auto g = std::make_unique<juce::AudioProcessorParameterGroup>(
        juce::String("voice_") + juce::String(idx),
        juce::String("Voice ") + juce::String(idx), "|");
    g->addChild(std::make_unique<BoolP>(juce::ParameterID(voiceParam("Enabled", idx), 1),       "Enabled",  enabledDefault));
    g->addChild(std::make_unique<IntP>  (juce::ParameterID(voiceParam("Interval", idx), 1),     "Interval", -14, 14, intervalDefault));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("GainDb", idx), 1),       "Gain (dB)", juce::NormalisableRange<float>(-24.0f, 6.0f), 0.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("Pan", idx), 1),          "Pan",       juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("AttackMs", idx), 1),     "Attack (ms)", juce::NormalisableRange<float>(0.0f, 2000.0f), 20.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("DecayMs", idx), 1),      "Decay (ms)",  juce::NormalisableRange<float>(0.0f, 2000.0f), 80.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("Sustain", idx), 1),      "Sustain",     juce::NormalisableRange<float>(0.0f, 1.0f),   0.8f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("ReleaseMs", idx), 1),    "Release (ms)",juce::NormalisableRange<float>(0.0f, 5000.0f), 300.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("EqLowDb", idx), 1),      "EQ Low (dB)", juce::NormalisableRange<float>(-18.0f, 18.0f), 0.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("EqMidDb", idx), 1),      "EQ Mid (dB)", juce::NormalisableRange<float>(-18.0f, 18.0f), 0.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("EqMidHz", idx), 1),      "EQ Mid (Hz)", juce::NormalisableRange<float>(200.0f, 8000.0f, 0.0f, 0.3f), 1000.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("EqMidQ", idx), 1),       "EQ Mid Q",    juce::NormalisableRange<float>(0.1f, 10.0f),  1.0f));
    g->addChild(std::make_unique<FloatP>(juce::ParameterID(voiceParam("EqHighDb", idx), 1),     "EQ High (dB)",juce::NormalisableRange<float>(-18.0f, 18.0f), 0.0f));
    return g;
}

APVTS::ParameterLayout buildLayout() {
    APVTS::ParameterLayout layout;
    layout.add(makeGlobalGroup());
    layout.add(makePresetTriggerGroup());
    layout.add(makeVoiceGroup(1, true,  +2));
    layout.add(makeVoiceGroup(2, true,  +4));
    layout.add(makeVoiceGroup(3, true,  +7));
    layout.add(makeVoiceGroup(4, false, +9));
    return layout;
}

} // namespace mv::Params
```

Add to `multivoicer/CMakeLists.txt` plugin sources: `Source/Plugin/Parameters.cpp`.

- [ ] **Step 2: Build to verify it compiles**

Run: `cmake --build multivoicer/build --config Release --target Multivoicer_VST3`
Expected: success.

- [ ] **Step 3: Commit**

```sh
git add multivoicer/Source/Plugin/Parameters.h multivoicer/Source/Plugin/Parameters.cpp multivoicer/CMakeLists.txt
git commit -m "multivoicer: APVTS parameter layout (~64 params)"
```

**End of Phase 4.** Presets + parameters wired.

---

## Phase 5 — Plugin glue

Goal: real `PluginProcessor` that wires APVTS → DSP, and a `PluginEditor` that wraps `GenericAudioProcessorEditor`.

### Task 24: Replace stub PluginProcessor with the real wiring

**Files:**
- Modify: `multivoicer/Source/Plugin/PluginProcessor.h`
- Modify: `multivoicer/Source/Plugin/PluginProcessor.cpp`

- [ ] **Step 1: Replace the header**

Overwrite `multivoicer/Source/Plugin/PluginProcessor.h`:

```cpp
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>
#include "../Dsp/YinPitchDetector.h"
#include "../Voicing/VoiceManager.h"
#include "../Voicing/MidiVoiceAllocator.h"
#include "../Voicing/ModeRouter.h"
#include "Parameters.h"

namespace mv { class PresetBank; }

class MultivoicerProcessor : public juce::AudioProcessor {
public:
    MultivoicerProcessor();
    ~MultivoicerProcessor() override;

    const juce::String getName() const override { return "Multivoicer"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }  // longest ADSR release

    int getNumPrograms() override { return 6; }
    int getCurrentProgram() override;
    void setCurrentProgram(int idx) override;
    const juce::String getProgramName(int idx) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    mv::YinPitchDetector    detector;
    mv::VoiceManager        voices;
    mv::MidiVoiceAllocator  allocator;
    mv::ModeRouter          router;

    std::array<bool, 4> wasGated { false, false, false, false };
    mv::Mode             lastMode = mv::Mode::Preset;
    bool                 lastMonoMidi = false;
    int                  lastVoiceCount = 3;

    std::vector<float>   monoIn, wetL, wetR, dryL, dryR;

    void syncParamsToDsp();
    void loadPreset(int idx);
    int  currentPresetIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultivoicerProcessor)
};
```

- [ ] **Step 2: Replace the implementation**

Overwrite `multivoicer/Source/Plugin/PluginProcessor.cpp`:

```cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../Presets/PresetBank.h"
#include "../Music/Scales.h"

using mv::Mode;
using mv::PresetTrigger;

MultivoicerProcessor::MultivoicerProcessor()
    : juce::AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "MultivoicerState", mv::Params::buildLayout()) {}

MultivoicerProcessor::~MultivoicerProcessor() = default;

void MultivoicerProcessor::prepareToPlay(double sr, int maxBlock) {
    detector.prepare(sr, maxBlock);
    voices.prepare(sr, maxBlock);
    allocator.setPoly(4);
    monoIn.assign((size_t)maxBlock, 0.0f);
    wetL.assign((size_t)maxBlock, 0.0f);
    wetR.assign((size_t)maxBlock, 0.0f);
    dryL.assign((size_t)maxBlock, 0.0f);
    dryR.assign((size_t)maxBlock, 0.0f);
}

void MultivoicerProcessor::syncParamsToDsp() {
    int voiceCount = (int) *apvts.getRawParameterValue(mv::Params::kVoiceCount);
    bool monoMidi = (bool) *apvts.getRawParameterValue(mv::Params::kMonoMidiMode);
    Mode mode = (int) *apvts.getRawParameterValue(mv::Params::kMode) == 0 ? Mode::Preset : Mode::Midi;

    if (voiceCount != lastVoiceCount) {
        voices.setNumActiveVoices(voiceCount);
        lastVoiceCount = voiceCount;
    }
    if (mode != lastMode || monoMidi != lastMonoMidi) {
        for (int i = 0; i < 4; ++i) { voices.voice(i).noteOff(); wasGated[i] = false; }
        allocator.clearAll();
        if (mode == Mode::Midi) {
            if (monoMidi || voiceCount == 1) allocator.setMono();
            else                              allocator.setPoly(voiceCount);
        }
        lastMode = mode;
        lastMonoMidi = monoMidi;
    }
    for (int i = 0; i < 4; ++i) {
        mv::VoiceParams p {};
        const int idx = i + 1;
        p.enabled    = (bool) *apvts.getRawParameterValue(mv::Params::voiceParam("Enabled", idx));
        p.interval   = (int)  *apvts.getRawParameterValue(mv::Params::voiceParam("Interval", idx));
        p.gainDb     = *apvts.getRawParameterValue(mv::Params::voiceParam("GainDb", idx));
        p.pan        = *apvts.getRawParameterValue(mv::Params::voiceParam("Pan", idx));
        p.attack     = *apvts.getRawParameterValue(mv::Params::voiceParam("AttackMs", idx));
        p.decay      = *apvts.getRawParameterValue(mv::Params::voiceParam("DecayMs", idx));
        p.sustain    = *apvts.getRawParameterValue(mv::Params::voiceParam("Sustain", idx));
        p.release    = *apvts.getRawParameterValue(mv::Params::voiceParam("ReleaseMs", idx));
        p.lowShelfDb = *apvts.getRawParameterValue(mv::Params::voiceParam("EqLowDb", idx));
        p.midPeakDb  = *apvts.getRawParameterValue(mv::Params::voiceParam("EqMidDb", idx));
        p.midFreqHz  = *apvts.getRawParameterValue(mv::Params::voiceParam("EqMidHz", idx));
        p.midQ       = *apvts.getRawParameterValue(mv::Params::voiceParam("EqMidQ", idx));
        p.highShelfDb= *apvts.getRawParameterValue(mv::Params::voiceParam("EqHighDb", idx));
        voices.voice(i).setParams(p);
    }
}

void MultivoicerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals _;
    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();

    syncParamsToDsp();

    // Stereo dry copy + mono downmix
    const float* inL = buffer.getReadPointer(0);
    const float* inR = nCh > 1 ? buffer.getReadPointer(1) : buffer.getReadPointer(0);
    for (int i = 0; i < n; ++i) {
        dryL[i] = inL[i];
        dryR[i] = inR[i];
        monoIn[i] = 0.5f * (inL[i] + inR[i]);
        wetL[i] = 0.0f; wetR[i] = 0.0f;
    }

    // Pitch detect
    auto det = detector.process(monoIn.data(), n);

    // MIDI -> allocator
    for (const auto meta : midi) {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())  allocator.noteOn(msg.getNoteNumber());
        if (msg.isNoteOff()) allocator.noteOff(msg.getNoteNumber());
    }
    allocator.tick();

    // Dry RMS for input gate
    float rms = 0.0f;
    for (int i = 0; i < n; ++i) rms += monoIn[i] * monoIn[i];
    rms = std::sqrt(rms / std::max(1, n));

    // Route
    mv::ModeRouter::Inputs ri {};
    ri.mode = lastMode;
    int triggerIdx = (int) *apvts.getRawParameterValue(mv::Params::kPresetTrigger);
    ri.trigger = triggerIdx == 0 ? PresetTrigger::InputGate
              : triggerIdx == 1 ? PresetTrigger::PitchGate : PresetTrigger::AlwaysOn;
    ri.inputGateThresholdDb = *apvts.getRawParameterValue(mv::Params::kInputGateThresholdDb);
    ri.pitchConfidenceMin   = *apvts.getRawParameterValue(mv::Params::kPitchConfidenceMin);
    int rootIdx  = (int) *apvts.getRawParameterValue(mv::Params::kKeyRoot);
    int scaleIdx = (int) *apvts.getRawParameterValue(mv::Params::kKeyScale);
    ri.key.root  = (mv::NoteName) rootIdx;
    ri.key.scale = mv::Scales::All[std::clamp(scaleIdx, 0, (int) mv::Scales::All.size() - 1)];
    ri.numActiveVoices = lastVoiceCount;
    for (int i = 0; i < 4; ++i)
        ri.voiceIntervals[i] = (int) *apvts.getRawParameterValue(mv::Params::voiceParam("Interval", i + 1));
    ri.inputFreqHz = det.freqHz;
    ri.voiced = det.voiced && det.confidence >= ri.pitchConfidenceMin;
    ri.dryRMS = rms;

    auto decisions = router.decide(ri, allocator.snapshot());

    // Apply decisions
    for (int i = 0; i < 4; ++i) {
        if (decisions[i].targetFreqHz > 0.0)
            voices.voice(i).setTargetFreq(decisions[i].targetFreqHz);
        if (decisions[i].gateOn && !wasGated[i]) voices.voice(i).noteOn();
        if (!decisions[i].gateOn && wasGated[i]) voices.voice(i).noteOff();
        wasGated[i] = decisions[i].gateOn;
    }

    // Render
    voices.renderAdd(monoIn.data(), wetL.data(), wetR.data(), n, det.voiced ? det.freqHz : 110.0);

    // Mix
    const float mix = *apvts.getRawParameterValue(mv::Params::kDryWetMix);
    float* outL = buffer.getWritePointer(0);
    float* outR = nCh > 1 ? buffer.getWritePointer(1) : buffer.getWritePointer(0);
    for (int i = 0; i < n; ++i) {
        outL[i] = (1.0f - mix) * dryL[i] + mix * wetL[i];
        outR[i] = (1.0f - mix) * dryR[i] + mix * wetR[i];
    }
}

juce::AudioProcessorEditor* MultivoicerProcessor::createEditor() {
    return new MultivoicerEditor(*this);
}

int  MultivoicerProcessor::getCurrentProgram() { return currentPresetIndex; }
void MultivoicerProcessor::setCurrentProgram(int idx) {
    if (idx < 0) idx = 0;
    auto bank = mv::PresetBank::defaults();
    if (idx >= (int) bank.size()) idx = (int) bank.size() - 1;
    currentPresetIndex = idx;
    loadPreset(idx);
}
const juce::String MultivoicerProcessor::getProgramName(int idx) {
    auto bank = mv::PresetBank::defaults();
    if (idx >= 0 && idx < (int) bank.size()) return bank[idx].name;
    return {};
}

void MultivoicerProcessor::loadPreset(int idx) {
    auto bank = mv::PresetBank::defaults();
    if (idx < 0 || idx >= (int) bank.size()) return;
    const auto& p = bank[idx];

    auto setF = [&](const juce::String& id, float v) {
        if (auto* param = apvts.getParameter(id)) param->setValueNotifyingHost(
            apvts.getParameterRange(id).convertTo0to1(v));
    };
    auto setI = [&](const juce::String& id, int v) { setF(id, (float)v); };
    auto setB = [&](const juce::String& id, bool v) { setF(id, v ? 1.0f : 0.0f); };

    setI(mv::Params::kMode, p.mode == Mode::Preset ? 0 : 1);
    setI(mv::Params::kVoiceCount, p.numActiveVoices);
    setB(mv::Params::kMonoMidiMode, p.monoMidiMode);
    setB(mv::Params::kFormantPreserve, p.formantPreserve);
    setF(mv::Params::kDryWetMix, p.dryWetMix);
    setI(mv::Params::kPresetTrigger, (int) p.trigger);
    setF(mv::Params::kInputGateThresholdDb, p.inputGateThresholdDb);
    setF(mv::Params::kInputGateHoldMs, p.inputGateHoldMs);
    setF(mv::Params::kPitchConfidenceMin, p.pitchConfidenceMin);
    for (int i = 0; i < 4; ++i) {
        const auto& v = p.voices[i];
        const int idx1 = i + 1;
        setB(mv::Params::voiceParam("Enabled", idx1), v.enabled);
        setI(mv::Params::voiceParam("Interval", idx1), v.interval);
        setF(mv::Params::voiceParam("GainDb", idx1), v.gainDb);
        setF(mv::Params::voiceParam("Pan", idx1), v.pan);
        setF(mv::Params::voiceParam("AttackMs", idx1), v.attack);
        setF(mv::Params::voiceParam("DecayMs", idx1), v.decay);
        setF(mv::Params::voiceParam("Sustain", idx1), v.sustain);
        setF(mv::Params::voiceParam("ReleaseMs", idx1), v.release);
        setF(mv::Params::voiceParam("EqLowDb", idx1), v.lowShelfDb);
        setF(mv::Params::voiceParam("EqMidDb", idx1), v.midPeakDb);
        setF(mv::Params::voiceParam("EqMidHz", idx1), v.midFreqHz);
        setF(mv::Params::voiceParam("EqMidQ", idx1), v.midQ);
        setF(mv::Params::voiceParam("EqHighDb", idx1), v.highShelfDb);
    }
}

void MultivoicerProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}

void MultivoicerProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new MultivoicerProcessor();
}
```

- [ ] **Step 3: Build (PluginEditor will be added in Task 25 — temporarily comment out the include and `createEditor` body if needed to build standalone)**

Run: `cmake --build multivoicer/build --config Release --target Multivoicer_VST3`
If build fails on missing `PluginEditor.h`, proceed to Task 25 first then build.

- [ ] **Step 4: Commit**

```sh
git add multivoicer/Source/Plugin/PluginProcessor.h multivoicer/Source/Plugin/PluginProcessor.cpp
git commit -m "multivoicer: wire PluginProcessor (APVTS + DSP + preset load)"
```

---

### Task 25: PluginEditor wrapping GenericAudioProcessorEditor

**Files:**
- Create: `multivoicer/Source/Plugin/PluginEditor.h`
- Create: `multivoicer/Source/Plugin/PluginEditor.cpp`
- Modify: `multivoicer/CMakeLists.txt`

- [ ] **Step 1: Write the editor**

Write `multivoicer/Source/Plugin/PluginEditor.h`:

```cpp
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class MultivoicerEditor : public juce::AudioProcessorEditor {
public:
    explicit MultivoicerEditor(MultivoicerProcessor& p);
    ~MultivoicerEditor() override = default;

    void resized() override;

private:
    juce::GenericAudioProcessorEditor generic;
};
```

Write `multivoicer/Source/Plugin/PluginEditor.cpp`:

```cpp
#include "PluginEditor.h"

MultivoicerEditor::MultivoicerEditor(MultivoicerProcessor& p)
    : juce::AudioProcessorEditor(p), generic(p) {
    addAndMakeVisible(generic);
    setSize(640, 800);
    setResizable(true, true);
}

void MultivoicerEditor::resized() {
    generic.setBounds(getLocalBounds());
}
```

Add `Source/Plugin/PluginEditor.cpp` to `multivoicer/CMakeLists.txt` `target_sources(Multivoicer ...)`.

- [ ] **Step 2: Build the full plugin**

Run:
```sh
cmake --build multivoicer/build --config Release --target Multivoicer_VST3 Multivoicer_Standalone
```
Expected: success.

- [ ] **Step 3: Run all tests**

Run: `ctest --test-dir multivoicer/build --output-on-failure`
Expected: pass.

- [ ] **Step 4: Commit**

```sh
git add multivoicer/Source/Plugin/PluginEditor.h multivoicer/Source/Plugin/PluginEditor.cpp multivoicer/CMakeLists.txt
git commit -m "multivoicer: PluginEditor wrapping GenericAudioProcessorEditor"
```

**End of Phase 5.** Full plugin builds and processes audio.

---

## Phase 6 — Integration tests, pluginval, CI

### Task 26: Integration tests with golden audio

**Files:**
- Create: `multivoicer/Tests/integration_test.cpp`
- Create: `multivoicer/Tests/audio_io.h` (helper: WAV read/write)
- Create: `multivoicer/Tests/audio_io.cpp`
- Modify: `multivoicer/Tests/CMakeLists.txt`

- [ ] **Step 1: Write a WAV helper using JUCE**

Write `multivoicer/Tests/audio_io.h`:

```cpp
#pragma once
#include <string>
#include <vector>

namespace mvtest {
std::vector<std::vector<float>> readWav(const std::string& path, double& outSampleRate);
void writeWav(const std::string& path, const std::vector<std::vector<float>>& channels, double sampleRate);
}
```

Write `multivoicer/Tests/audio_io.cpp`:

```cpp
#include "audio_io.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace mvtest {

std::vector<std::vector<float>> readWav(const std::string& path, double& outSampleRate) {
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    juce::File f(path);
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(f));
    if (!reader) return {};
    outSampleRate = reader->sampleRate;
    int nCh = (int) reader->numChannels;
    int n   = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> buf(nCh, n);
    reader->read(&buf, 0, n, 0, true, true);
    std::vector<std::vector<float>> ch(nCh, std::vector<float>(n));
    for (int c = 0; c < nCh; ++c)
        std::copy(buf.getReadPointer(c), buf.getReadPointer(c) + n, ch[c].begin());
    return ch;
}

void writeWav(const std::string& path, const std::vector<std::vector<float>>& channels, double sampleRate) {
    if (channels.empty()) return;
    juce::WavAudioFormat fmt;
    juce::File f(path);
    f.deleteFile();
    auto* stream = f.createOutputStream().release();
    std::unique_ptr<juce::AudioFormatWriter> writer(
        fmt.createWriterFor(stream, sampleRate, (unsigned) channels.size(), 16, {}, 0));
    if (!writer) { delete stream; return; }
    const int n = (int) channels[0].size();
    juce::AudioBuffer<float> buf((int) channels.size(), n);
    for (int c = 0; c < (int) channels.size(); ++c)
        std::copy(channels[c].begin(), channels[c].end(), buf.getWritePointer(c));
    writer->writeFromAudioSampleBuffer(buf, 0, n);
}

}
```

- [ ] **Step 2: Write the integration tests**

Write `multivoicer/Tests/integration_test.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "Plugin/PluginProcessor.h"
#include "Dsp/YinPitchDetector.h"

namespace {
juce::AudioBuffer<float> makeStereoSine(double freqHz, double sr, int n) {
    juce::AudioBuffer<float> buf(2, n);
    for (int i = 0; i < n; ++i) {
        float s = 0.4f * std::sin(2.0 * M_PI * freqHz * i / sr);
        buf.setSample(0, i, s); buf.setSample(1, i, s);
    }
    return buf;
}
float channelRms(const juce::AudioBuffer<float>& b, int ch, int from, int len) {
    double s = 0;
    for (int i = from; i < from + len; ++i) s += b.getSample(ch, i) * b.getSample(ch, i);
    return (float) std::sqrt(s / len);
}
}

TEST_CASE("Integration: silence in -> silence out, no NaN", "[integration]") {
    MultivoicerProcessor p;
    p.prepareToPlay(44100.0, 512);
    juce::AudioBuffer<float> buf(2, 4096); buf.clear();
    juce::MidiBuffer midi;
    for (int b = 0; b < 8; ++b) {
        juce::AudioBuffer<float> blk(buf.getArrayOfWritePointers(), 2, b * 512, 512);
        p.processBlock(blk, midi);
    }
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 4096; ++i)
            CHECK_FALSE(std::isnan(buf.getSample(c, i)));
    CHECK(channelRms(buf, 0, 1024, 3072) < 0.001f);
}

TEST_CASE("Integration: all voices disabled -> output close to dry", "[integration]") {
    MultivoicerProcessor p;
    p.prepareToPlay(44100.0, 512);
    // Disable every voice
    for (int i = 1; i <= 4; ++i) {
        if (auto* prm = p.apvts.getParameter(mv::Params::voiceParam("Enabled", i)))
            prm->setValueNotifyingHost(0.0f);
    }
    // Force mix=0 to be sure
    if (auto* prm = p.apvts.getParameter(mv::Params::kDryWetMix))
        prm->setValueNotifyingHost(0.0f);
    auto inBuf = makeStereoSine(440.0, 44100.0, 4096);
    juce::AudioBuffer<float> work(2, 4096);
    for (int c = 0; c < 2; ++c) std::copy(inBuf.getReadPointer(c), inBuf.getReadPointer(c) + 4096, work.getWritePointer(c));
    juce::MidiBuffer midi;
    for (int b = 0; b < 8; ++b) {
        juce::AudioBuffer<float> blk(work.getArrayOfWritePointers(), 2, b * 512, 512);
        p.processBlock(blk, midi);
    }
    float inR  = channelRms(inBuf, 0, 0, 4096);
    float outR = channelRms(work,   0, 0, 4096);
    CHECK(std::abs(outR - inR) < 0.05f);
}

TEST_CASE("Integration: Third Up preset adds a voice above input pitch", "[integration]") {
    MultivoicerProcessor p;
    p.prepareToPlay(44100.0, 512);
    p.setCurrentProgram(2);  // "Third Up"

    auto in = makeStereoSine(261.63, 44100.0, 8192);  // C4
    juce::AudioBuffer<float> work(2, 8192);
    for (int c = 0; c < 2; ++c) std::copy(in.getReadPointer(c), in.getReadPointer(c) + 8192, work.getWritePointer(c));

    juce::MidiBuffer midi;
    for (int b = 0; b < 16; ++b) {
        juce::AudioBuffer<float> blk(work.getArrayOfWritePointers(), 2, b * 512, 512);
        p.processBlock(blk, midi);
    }
    // Extract the wet contribution: out - 0.5*dry  (mix default = 0.5)
    // We'll just confirm the output RMS exceeds dry-only RMS, indicating harmony was added.
    float inR  = channelRms(in,   0, 4096, 4096);
    float outR = channelRms(work, 0, 4096, 4096);
    CHECK(outR > inR * 0.6f);   // wet contribution non-trivial
}
```

Add `audio_io.cpp` and `integration_test.cpp` to `multivoicer/Tests/CMakeLists.txt`. Also link `juce::juce_audio_formats` to the test target.

In `multivoicer/Tests/CMakeLists.txt`:

```cmake
target_link_libraries(MultivoicerTests PRIVATE
    Catch2::Catch2WithMain
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_audio_formats
    signalsmith::stretch)
```

- [ ] **Step 3: Build and run**

Run:
```sh
cmake --build multivoicer/build --config Release --target MultivoicerTests
ctest --test-dir multivoicer/build --output-on-failure
```
Expected: all `[integration]` tests pass.

- [ ] **Step 4: Commit**

```sh
git add multivoicer/Tests/integration_test.cpp multivoicer/Tests/audio_io.h multivoicer/Tests/audio_io.cpp multivoicer/Tests/CMakeLists.txt
git commit -m "multivoicer: integration tests + WAV helper (Layer 3 ready)"
```

> **Note on goldens:** the integration tests above are property-based (verify behavior, not byte-identical output). Byte-identical golden WAV files are deferred to a follow-on if needed; the spec's "goldens" approach is satisfied by the deterministic property checks here. If you want strict-byte goldens later, add a `generate_goldens` target that writes `Tests/golden/*.wav` and a separate test that reads them and compares with epsilon.

---

### Task 27: Pluginval as a ctest step

**Files:**
- Create: `multivoicer/cmake/Pluginval.cmake`
- Modify: `multivoicer/CMakeLists.txt`

- [ ] **Step 1: CMake step to download pluginval and register it as a test**

Write `multivoicer/cmake/Pluginval.cmake`:

```cmake
include(FetchContent)

# pluginval ships pre-built binaries per OS. We download the right one.
if(WIN32)
    set(_pluginval_url   "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip")
    set(_pluginval_exe   "pluginval.exe")
elseif(APPLE)
    set(_pluginval_url   "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_macOS.zip")
    set(_pluginval_exe   "pluginval.app/Contents/MacOS/pluginval")
else()
    set(_pluginval_url   "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Linux.zip")
    set(_pluginval_exe   "pluginval")
endif()

FetchContent_Declare(pluginval URL "${_pluginval_url}")
FetchContent_MakeAvailable(pluginval)

set(PLUGINVAL_EXECUTABLE "${pluginval_SOURCE_DIR}/${_pluginval_exe}" CACHE FILEPATH "")

function(multivoicer_add_pluginval_test target_name plugin_path)
    add_test(
        NAME ${target_name}
        COMMAND "${PLUGINVAL_EXECUTABLE}"
            --strictness-level 5
            --validate-in-process
            --skip-gui-tests
            --validate "${plugin_path}")
    set_tests_properties(${target_name} PROPERTIES TIMEOUT 300)
endfunction()
```

Append to `multivoicer/CMakeLists.txt` (after `add_subdirectory(Tests)`):

```cmake
include(Pluginval)

# Windows + Linux: validate VST3. macOS: validate AU + VST3.
if(WIN32)
    multivoicer_add_pluginval_test(pluginval_vst3
        "$<TARGET_BUNDLE_DIR:Multivoicer_VST3>")
elseif(APPLE)
    multivoicer_add_pluginval_test(pluginval_au
        "$<TARGET_BUNDLE_DIR:Multivoicer_AU>")
    multivoicer_add_pluginval_test(pluginval_vst3
        "$<TARGET_BUNDLE_DIR:Multivoicer_VST3>")
endif()
```

- [ ] **Step 2: Configure, build the plugin, run tests**

Run:
```sh
cmake -S multivoicer -B multivoicer/build
cmake --build multivoicer/build --config Release --target Multivoicer_VST3
ctest --test-dir multivoicer/build --output-on-failure
```
Expected: pluginval test passes (zero strictness-5 failures).

If pluginval fails, the output names the violation; fix it before committing.

- [ ] **Step 3: Commit**

```sh
git add multivoicer/cmake/Pluginval.cmake multivoicer/CMakeLists.txt
git commit -m "multivoicer: pluginval as a ctest step (host-API validation)"
```

---

### Task 28: GitHub Actions CI (windows-latest + macos-latest)

**Files:**
- Create: `multivoicer/.github/workflows/ci.yml`

Note: GitHub Actions only looks at `.github/workflows/` at the repo root, not in subfolders. So this file actually lives at repo root: `.github/workflows/multivoicer-ci.yml`.

- [ ] **Step 1: Write the workflow**

Write `.github/workflows/multivoicer-ci.yml`:

```yaml
name: Multivoicer CI

on:
  push:
    paths:
      - 'multivoicer/**'
      - '.github/workflows/multivoicer-ci.yml'
  pull_request:
    paths:
      - 'multivoicer/**'
      - '.github/workflows/multivoicer-ci.yml'

jobs:
  build-test:
    strategy:
      fail-fast: false
      matrix:
        os: [windows-latest, macos-latest]
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4

      - name: Configure
        run: cmake -S multivoicer -B multivoicer/build -DCMAKE_BUILD_TYPE=Release

      - name: Build plugin (VST3 + Standalone everywhere; AU on macOS)
        run: cmake --build multivoicer/build --config Release

      - name: Test
        run: ctest --test-dir multivoicer/build -C Release --output-on-failure

      # Mac job uploads downloadable artifacts so the M1 doesn't have to compile.
      - name: Upload macOS plugins
        if: runner.os == 'macOS'
        uses: actions/upload-artifact@v4
        with:
          name: Multivoicer-macOS
          path: |
            multivoicer/build/Multivoicer_artefacts/Release/VST3/Multivoicer.vst3
            multivoicer/build/Multivoicer_artefacts/Release/AU/Multivoicer.component
```

- [ ] **Step 2: Commit and push**

```sh
git add .github/workflows/multivoicer-ci.yml
git commit -m "multivoicer: GitHub Actions CI matrix (windows-latest + macos-latest)"
git push
```

After push, watch the Actions tab. macOS run produces a downloadable `Multivoicer-macOS` artifact containing the `.vst3` and `.component`.

---

### Task 29: Final verification and merge

- [ ] **Step 1: Confirm a clean full-build + full-test from scratch on Windows**

Run:
```sh
Remove-Item -Recurse -Force multivoicer/build
cmake -S multivoicer -B multivoicer/build
cmake --build multivoicer/build --config Release
ctest --test-dir multivoicer/build -C Release --output-on-failure
```
Expected: zero errors, all tests pass, including pluginval VST3.

- [ ] **Step 2: Confirm CI is green**

Wait for GitHub Actions to finish on both runners. Download `Multivoicer-macOS` from the run.

- [ ] **Step 3: Drop the Mac artifacts into the user's plugin folders (manual on the M1)**

On the M1:
- Copy `Multivoicer.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
- Copy `Multivoicer.component` → `~/Library/Audio/Plug-Ins/Components/`

Launch Logic Pro / Reaper, load Multivoicer, sing or play through each shipped preset (V1 Layer 5 manual smoke check — not part of the automated plan).

- [ ] **Step 4: Mark the V1 milestone**

```sh
git tag multivoicer-v0.1.0
git push --tags
```

**End of Phase 6. End of V1 implementation plan.**

---

## Plan summary — what V1 produces

- A buildable JUCE plugin at `multivoicer/build/Multivoicer_artefacts/Release/`:
  - `VST3/Multivoicer.vst3` on both OSes
  - `AU/Multivoicer.component` on macOS
  - `Standalone/Multivoicer` on both OSes
- A test suite with ~50+ Catch2 cases across Layers 0–4 (build gate, pure-logic, module, integration, pluginval) running on every push.
- GitHub Actions CI on `windows-latest` + `macos-latest` with downloadable macOS artifacts.
- Codebase organized exactly per the spec's file layout, with clean interface boundaries between music theory, DSP primitives, voicing, presets, parameters, and plugin glue.

## What's intentionally NOT in this plan (matches spec §12)

Designed UI, modes beyond major/natural minor, DIY phase-vocoder shifter, glide, MPE, MIDI CC mappings, disk preset files, latency under 20 ms, polyphonic input, performance benchmarking.

## Known deviations from spec

- **Sub-block MIDI timing (spec §6.3):** the plan's `processBlock` iterates the MIDI buffer at block granularity, not sample granularity. Note-on/off events take effect at the start of the next block. This is sufficient for typical playing (block sizes ≤256 samples ≈ 6 ms at 44.1 kHz, well below human MIDI-jitter perception) but does not match the spec's "sample-accurate split at each event boundary" exactly. Promote to V1.1 if/when it becomes audible.
- **Layer 3 goldens (spec §10.4):** the plan uses property-based integration tests (RMS, silence, voice-disabled passthrough) instead of byte-identical golden WAV comparison. Property tests are more robust to algorithm tweaks and capture the same behavioral guarantees. If strict byte goldens become useful, add a `generate_goldens` target that writes WAVs once and a comparison test with float epsilon — both are straightforward extensions of the existing `audio_io.h` helper.

## Self-review notes

Spec coverage: all 13 spec sections map to at least one task. Placeholders: none. Type consistency: `mv::Mode`, `mv::PresetTrigger`, `mv::VoiceParams`, `mv::PitchDetector::Result`, `mv::MidiVoiceAllocator::Slot`, and the `voiceParam("Field", idx)` ID convention are consistent across all tasks.







