# Multivoicer Plugin — Design Spec

**Date:** 2026-05-28
**Status:** Draft pending user review
**Scope:** V1 of the `multivoicer` project inside `AudioDevProjects/`.

---

## 1. Goal

A JUCE audio plugin that takes a monophonic audio input (guitar or vocals) and produces up to 4 pitch-shifted harmony voices summed against the dry signal. Two control modes: a preset-driven diatonic harmonizer (user picks key + scale + per-voice diatonic interval) and a MIDI-driven mode where held MIDI notes determine harmony pitches. A monophonic 1-voice configuration with formant preservation enables a "talkbox" use case.

Target plugin formats: **VST3** and **AU**, built for **macOS arm64** (M1). Built with **JUCE** and **CMake**.

V1 of this project intentionally favors functionality over UI polish: the editor uses `juce::GenericAudioProcessorEditor`. A designed UI is a follow-on milestone.

## 2. High-level architecture

```
                 +--------------------+
   Audio in ---> | PluginProcessor    | <--- MIDI in
                 +---------+----------+
                           |
              +------------v-------------+
              |  Per-block process loop  |
              +------------+-------------+
                           |
        +------------------+------------------+
        |                                     |
+-------v--------+                 +----------v-----------+
| PitchDetector  |                 | ModeRouter           |
| (DIY YIN)      |                 | (Preset | MIDI)      |
+-------+--------+                 +----------+-----------+
        |                                     |
        | inputF0 (Hz) + confidence           | target requests per voice
        |                                     | (pitch target + gate state)
        +-------------------+-----------------+
                            |
                  +---------v----------+
                  |  VoiceManager      |
                  |  (4 Voice slots)   |
                  +---------+----------+
                            |
                +-----------+-----------+
                |   per voice           |
                |   PitchShifter        |
                |   ADSR                |
                |   3-band IIR EQ       |
                |   gain + pan          |
                +-----------+-----------+
                            |
                   sum -> dry/wet mix -> output
```

**Modules and their single responsibilities:**

- `PluginProcessor` — JUCE `AudioProcessor`. Owns APVTS, MIDI, the per-block loop. No DSP internals.
- `PitchDetector` (interface) + `YinPitchDetector` (impl) — mono block in, fundamental frequency + voiced flag out. Stateful, swappable.
- `PitchShifter` (interface) + `SignalsmithPitchShifter` (impl) — block + shift ratio in, shifted block out. Stateful, swappable. One per voice.
- `MusicTheory` — pure functions and types: `NoteName`, `PitchClassSet`, `Key`, `quantizeToKey`, `targetForInterval`, `scaleDegreeOf`. No audio dependency.
- `Scales` — the single registry of `ScaleDef`s. Adding a scale = adding one entry.
- `ModeRouter` — given mode, preset, MIDI state, detected pitch → `VoiceDecision[]` (target freq + gate state per voice).
- `MidiVoiceAllocator` — synth-style polyphonic allocator with voice stealing; mono-mode flag for legato.
- `VoiceManager` — owns the 4 `Voice` instances, applies routing, sums output.
- `Voice` — one harmony voice: shifter + ADSR + EQ + gain/pan. Adds into the stereo wet bus.
- `PresetManager` + `PresetBank` — `Preset` struct definition and the 6 shipped default presets.

## 3. DSP libraries (V1)

- **Pitch shifter:** [Signalsmith Stretch](https://signalsmith-audio.co.uk/code/stretch/) — BSD-3, modern C++, formant-preserving, latency-tunable. Wrapped behind `PitchShifter` interface so a DIY phase-vocoder/PSOLA implementation can replace it later without touching the rest of the codebase.
- **Pitch detector:** DIY YIN implementation (~150 LOC). YIN is the canonical real-time monophonic pitch detector — autocorrelation-based, well-documented, ideal for learning. No external dependency. Behind a `PitchDetector` interface for the same swap-later pattern.
- **EQ, ADSR, filters, math:** `juce_dsp` (`juce::dsp::IIR::Filter`, `juce::ADSR`, `juce::dsp::FFT` if needed by YIN).

## 4. Core types and interfaces

### 4.1 Music theory

```cpp
enum class NoteName : int { C=0, Cs, D, Ds, E, F, Fs, G, Gs, A, As, B };

struct PitchClassSet {
    uint16_t mask;  // 12 LSBs: bit i set iff semitone i above root is in the scale
    constexpr bool contains(int semitone) const { return (mask >> (semitone % 12)) & 1; }
};

struct ScaleDef {
    const char* name;
    PitchClassSet intervals;
};

namespace Scales {
    constexpr ScaleDef Major        { "Major",         {0b0000'1010'1101'0101} }; // 0,2,4,5,7,9,11
    constexpr ScaleDef NaturalMinor { "Natural Minor", {0b0000'1010'1101'1001} }; // 0,2,3,5,7,8,10
    constexpr std::array<const ScaleDef*, 2> All { &Major, &NaturalMinor };
}

struct Key { NoteName root; const ScaleDef* scale; };

int    midiFromFreq(double hz);                          // 69 + 12*log2(hz/440)
double freqFromMidi(double midi);
int    quantizeToKey(int midiNote, Key);                 // snap to nearest in-key note
int    scaleDegreeOf(int midiNote, Key);                 // 0=root, 1=2nd, ..., -1 if not in key
int    targetForInterval(int inputMidi, int diatonicInterval, Key);
```

`diatonicInterval` is a **scale-degree count**, not semitones. `+2` from C in C-major is E (major 3rd in semitones); `+2` from D in C-major is F (minor 3rd in semitones). This is the diatonic-harmonizer behavior. Negative values shift below.

### 4.2 DSP interfaces

```cpp
class PitchDetector {
public:
    struct Result { double freqHz; float confidence; bool voiced; };
    virtual ~PitchDetector() = default;
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;
    virtual Result process(const float* mono, int numSamples) = 0;
};

class PitchShifter {
public:
    virtual ~PitchShifter() = default;
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;
    virtual void setShiftRatio(double ratio) = 0;     // output_freq / input_freq
    virtual void setFormantPreserve(bool on) = 0;
    virtual void process(const float* in, float* out, int numSamples) = 0;
};
```

### 4.3 Voice and routing

```cpp
struct VoiceParams {
    bool   enabled;
    int    interval;       // diatonic scale-degree interval (preset mode)
    float  gainDb;
    float  pan;            // -1..+1
    float  attack, decay, sustain, release;   // ADSR
    float  lowShelfDb, midPeakDb, highShelfDb;
    float  midFreqHz, midQ;
};

class Voice {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void setTargetFreq(double hz);
    void noteOn();
    void noteOff();
    void setParams(const VoiceParams&);
    void renderAdd(const float* dryMono, float* outL, float* outR, int n, double inputFreqHz);
private:
    std::unique_ptr<PitchShifter> shifter;
    juce::ADSR adsr;
    // 3-band EQ sections...
};

class VoiceManager {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void setNumActiveVoices(int n);     // 1..4
    Voice& voice(int i);
    void renderAdd(const float* dry, float* outL, float* outR, int n, double inputFreqHz);
};

enum class Mode { Preset, Midi };
enum class PresetTrigger { InputGate, PitchGate, AlwaysOn };

struct VoiceDecision {
    bool   active;        // false = skip render entirely (voice slot unused)
    double targetFreqHz;  // shift target when active
    bool   gateOn;        // true = ADSR in note-on state; false = in release
};

class ModeRouter {
public:
    std::array<VoiceDecision, 4> decide(/* mode, preset, allocator state, input pitch, dry RMS */);
};
```

## 5. Per-block processing flow

```
1. Downmix to mono                       monoIn = 0.5 * (L + R)
                                         (keep dry stereo for the wet/dry mix)

2. Pitch detect                          d = detector.process(monoIn, n)
                                         inputFreq, voiced = d.freqHz, d.voiced

3. MIDI parse                            allocator.processMidi(midiBuffer)
                                         -> per-voice MIDI targets (or none)

4. Mode route                            decisions = router.decide(
                                             mode, preset, allocatorState,
                                             inputFreq, voiced, dryRMS)

5. Apply decisions (edge detect)         for i: set target; emit noteOn/noteOff
                                         on rising/falling gate

6. Render voices                         voiceManager.renderAdd(monoIn, wetL, wetR, n, inputFreq)

7. Mix                                   out = (1 - mix) * dry + mix * wet
```

### 5.1 Realtime safety

- No allocations on the audio thread. All buffers (`monoIn`, `wetL`, `wetR`, per-voice `tmp`) are owned and sized in `prepareToPlay`.
- Parameter changes flow through `juce::AudioProcessorValueTreeState` (lock-free). Each `Voice` snapshots its `VoiceParams` at block start.
- Preset load runs on the message thread and writes through APVTS — never directly into audio-thread objects.
- Mode/scale changes are enum/index swaps; no DSP graph rebuilds.

### 5.2 Edge cases (explicit)

- **Unvoiced input** (detector reports `voiced=false`):
  - MIDI mode: keep current MIDI targets; ADSR is gated by MIDI note-on/off, not voicing. (You can hold a chord across a breath.)
  - Preset mode + `PitchGate` trigger: release voices.
  - Preset mode + `InputGate` trigger: use `dryRMS` against threshold instead.
- **Input out of detector range** (e.g., <50 Hz or >2 kHz): treated as `voiced=false`.
- **Voice count change at runtime** (1↔4): voices being disabled fire `noteOff` so ADSR releases naturally; newly enabled voices start in idle.
- **Mono ↔ poly toggle while notes held:** flush allocator (all notes off) then re-route under new mode.
- **Preset ↔ MIDI mode toggle while voices are active:** issue `noteOff` to all voices so ADSR releases naturally; allocator state is cleared. The mode toggle is *not* sample-accurate — it takes effect at the next block boundary.

### 5.3 Detector latency

YIN needs ~`2/f` seconds of audio to detect frequency `f`. For 80 Hz (low guitar/voice), that's ~25 ms ≈ 1100 samples @ 44.1 kHz. Detector keeps a sliding ring buffer and runs analysis in fixed hops (512 samples) regardless of host block size. Detector latency ≈ one hop (~10 ms @ 44.1 kHz). Combined with Signalsmith Stretch's tunable latency, total stays within the 20–50 ms budget for V1; targeting <20 ms is a tuning milestone, not a V1 ship requirement.

## 6. MIDI handling

### 6.1 Mono mode (voice count = 1)

Last-note-priority with held-note stack:
- Note-on: push to stack; target = this note; trigger attack if previously released.
- Note-off: pop. If stack non-empty, target = new top, **no re-attack** (legato — ADSR stays in sustain). If stack empty, trigger release.
- No glide/portamento in V1. Pitch jumps instantly between notes.

### 6.2 Poly mode (voice count 2–4)

`MidiVoiceAllocator`:
- 4 slots, each tracks `{ midiNote, ageCounter, free }`.
- Note-on: pick lowest-index free slot, else steal the slot with greatest age.
- Note-off: free the matching slot; ignore if already stolen.
- Slots `≥ numVoices` are tracked but functionally disabled (simpler than reconfiguring on every voice-count change).

### 6.3 Sub-block timing

MIDI events have sample offsets in JUCE's buffer. The block is split at each event boundary so a note-on at sample 30 of a 256-sample block triggers attack at sample 30, not at sample 0.

### 6.4 Channels & CC

- V1 listens to all MIDI channels. MPE / per-channel routing is V2.
- **Program Change** maps 1:1 into the loaded preset bank. Useful for foot controllers.
- **CC** not wired in V1. DAW-side MIDI learn is free for the user.

### 6.5 Preset mode

No MIDI involvement. The router pulls targets entirely from the preset + detected input pitch. MIDI buffer is consumed-and-ignored (the plugin doesn't pass MIDI through — it's an audio plugin).

## 7. Preset model

### 7.1 Preset structure

```cpp
struct Preset {
    juce::String name;

    // Mode-independent
    Key            key;
    int            numActiveVoices;   // 1..4
    bool           formantPreserve;
    float          dryWetMix;         // 0..1

    // Preset-mode specifics
    PresetTrigger  trigger;
    float          inputGateThresholdDb;
    float          inputGateHoldMs;
    float          pitchConfidenceMin;

    // Per-voice
    std::array<VoiceParams, 4> voices;
};
```

### 7.2 Default preset bank (shipped V1)

| # | Name              | Mode    | Voices                       | Notes                              |
|---|-------------------|---------|------------------------------|------------------------------------|
| 1 | Octave Up         | preset  | 1 voice, +7 diatonic         | Simplest sanity check              |
| 2 | Octave Down       | preset  | 1 voice, -7 diatonic         | Pairs with #1                      |
| 3 | Third Up          | preset  | 1 voice, +2 diatonic         | Classic harmonizer setting         |
| 4 | 3rd + 5th Above   | preset  | 2 voices: +2, +4             | Triad on top                       |
| 5 | Full Triad        | preset  | 3 voices: +2, +4, +7         | Doubled chord on top               |
| 6 | Talkbox Mono      | MIDI    | 1 voice, mono mode, formant on | Talkbox/vocoder demo             |

Presets 1–5 default to `Key{C, &Scales::Major}` with `InputGate` trigger. The user picks the actual key at runtime — presets define the *relationship* (intervals, voice count, envelope, EQ), not the key.

**Preset load semantics:** loading a preset is an atomic APVTS state replace — it overwrites `keyRoot`/`keyScale` along with every other parameter. The user is expected to re-set `keyRoot`/`keyScale` after loading if they're working in a different key. A future iteration could expose a "preserve key on preset change" toggle, but V1 keeps load behavior simple.

### 7.3 Storage

- **Default bank:** compiled-in via a `static` factory in `PresetBank.cpp`.
- **Active state:** persisted by `juce::AudioProcessorValueTreeState::state` (the DAW saves all parameters automatically). No standalone `.preset` files in V1.
- **Switching presets:** UI dropdown does an atomic APVTS state replace, not 30 individual parameter sets. Instant switch (no smoothing) in V1.

### 7.4 Out of scope for V1

- On-disk preset files / preset sharing between plugin instances.
- Preset save/rename UI (you tweak; the DAW persists).
- Preset categories/tags.

## 8. Parameter surface

All parameters in `juce::AudioProcessorValueTreeState`, automatable. ~64 total.

### 8.1 Global

| ID | Type | Range | Default |
|---|---|---|---|
| `mode` | choice | Preset, MIDI | Preset |
| `voiceCount` | int | 1–4 | 3 |
| `keyRoot` | choice | C, C#, …, B (12) | C |
| `keyScale` | choice | Major, NaturalMinor | Major |
| `formantPreserve` | bool | — | true |
| `dryWetMix` | float | 0–1 | 0.5 |
| `preset` | choice | preset bank names | "Octave Up" |
| `monoMidiMode` | bool | — | false |

### 8.2 Preset-mode specifics

| ID | Type | Range | Default |
|---|---|---|---|
| `presetTrigger` | choice | InputGate, PitchGate, AlwaysOn | InputGate |
| `inputGateThresholdDb` | float | -60..0 dB | -40 |
| `inputGateHoldMs` | float | 0–500 ms | 50 |
| `pitchConfidenceMin` | float | 0–1 | 0.5 |

### 8.3 Per voice (×4, suffix `_1`..`_4`)

| ID | Type | Range | Default (v1/v2/v3/v4) |
|---|---|---|---|
| `voiceEnabled_N` | bool | — | t/t/t/f |
| `voiceInterval_N` | int | -14..+14 (diatonic) | +2/+4/+7/+9 |
| `voiceGainDb_N` | float | -24..+6 | 0 |
| `voicePan_N` | float | -1..+1 | 0 |
| `voiceAttackMs_N` | float | 0–2000 | 20 |
| `voiceDecayMs_N` | float | 0–2000 | 80 |
| `voiceSustain_N` | float | 0–1 | 0.8 |
| `voiceReleaseMs_N` | float | 0–5000 | 300 |
| `voiceEqLowDb_N` | float | -18..+18 | 0 |
| `voiceEqMidDb_N` | float | -18..+18 | 0 |
| `voiceEqMidHz_N` | float | 200–8000 | 1000 |
| `voiceEqMidQ_N` | float | 0.1–10 | 1.0 |
| `voiceEqHighDb_N` | float | -18..+18 | 0 |

### 8.4 APVTS grouping

```
root
├── global
├── presetTrigger
├── voice_1
├── voice_2
├── voice_3
└── voice_4
```

V1.5 UI work can lay these out without changing IDs.

## 9. File and module layout

```
AudioDevProjects/
  multivoicer/
    CMakeLists.txt
    README.md
    .gitignore
    cmake/
      JUCE.cmake                          # FetchContent JUCE
      Signalsmith.cmake                   # FetchContent signalsmith-stretch
      Catch2.cmake                        # FetchContent Catch2 v3
      Pluginval.cmake                     # download pluginval binary on demand
    Source/
      Plugin/
        PluginProcessor.h / .cpp
        PluginEditor.h / .cpp             # wraps GenericAudioProcessorEditor
        Parameters.h / .cpp               # APVTS layout + IDs
      Music/
        MusicTheory.h / .cpp
        Scales.h                          # the registry
      Dsp/
        PitchDetector.h
        YinPitchDetector.h / .cpp
        PitchShifter.h
        SignalsmithPitchShifter.h / .cpp
        VoiceEq.h / .cpp
      Voicing/
        Voice.h / .cpp
        VoiceManager.h / .cpp
        ModeRouter.h / .cpp
        MidiVoiceAllocator.h / .cpp
      Presets/
        Preset.h
        PresetBank.h / .cpp
    Tests/
      CMakeLists.txt
      music_theory_test.cpp
      yin_detector_test.cpp
      pitch_shifter_test.cpp
      voice_test.cpp
      voice_eq_test.cpp
      voice_allocator_test.cpp
      mode_router_test.cpp
      preset_bank_test.cpp
      integration_test.cpp                # full processBlock + golden audio
      golden/
        # small .wav reference files
    .github/
      workflows/
        ci.yml                            # windows-latest + macos-latest matrix
```

### 9.1 CMake topology

One target `Multivoicer` (the plugin) plus one target `MultivoicerTests` (the test exe). JUCE / Signalsmith / Catch2 pulled via `FetchContent` — no submodules, no `External/` checkout. Pluginval downloaded on demand by a CMake-driven step.

Plugin formats built: `VST3`, `Standalone` everywhere; `AU` only on macOS (`if(APPLE)`).

### 9.2 What this layout deliberately omits

- No shared library across `AudioDevProjects`. If a future project needs `MusicTheory`, copy it; promote to a shared library only when a third caller appears.
- No `core` library target separate from the plugin target. Splitting only adds friction at V1 scope.
- No `include/` vs `src/` split. Headers live next to their `.cpp`. JUCE convention.

## 10. Testing strategy

The constraint: **manual ear-checking only happens on the M1 Mac.** Tests must verify correctness numerically, headless. Manual listening is the *final* gate, never the only gate.

**Automation split:** Layers 0–4 are fully automated (run via `ctest` on every push, both Windows and macOS CI). Layer 5 is the only manual layer — a human listening on the M1 Mac to artifacts produced by macOS CI.

### 10.1 Layer 0 — Build gate (Automated)

The plugin and the test executable are both `ctest` dependencies. If `Multivoicer` fails to compile, `ctest` exits non-zero. Runs on every platform's CI.

### 10.2 Layer 1 — Pure-logic tests (Automated)

- `music_theory_test`:
  - `midiFromFreq(440) == 69`; round-trip with `freqFromMidi`.
  - `quantizeToKey(70, {C, Major})` snaps deterministically to nearest in-key note.
  - `targetForInterval(C4, +2, {C, Major}) == E4`; `targetForInterval(D4, +2, {C, Major}) == F4`.
  - `targetForInterval(C4, +2, {A, NaturalMinor}) == Eb4`.
  - Off-key input: `targetForInterval(C#4, +2, {C, Major})` snaps C#→C then +2→E4.
  - Intervals beyond one octave.
- `mode_router_test`:
  - Preset + AlwaysOn + voiced → all enabled voices active, targets correct.
  - Preset + PitchGate + unvoiced → all `gateOn=false`.
  - Preset + InputGate + dryRMS below threshold → `gateOn=false`; hold time honored.
  - MIDI mode + no notes → all `gateOn=false`.
  - MIDI mode + 2 notes held, 4 voices enabled → 2 active, 2 idle.
  - Voice-count change: turned-off voices emit exactly one `noteOff`.
- `voice_allocator_test`:
  - Single note on/off → slot 0.
  - 5th note steals oldest slot.
  - Mono: stack push/pop, legato on stacked release.
  - Stolen note's note-off ignored.
  - Sub-block sample-offset events.
- `preset_bank_test`:
  - All 6 presets construct valid `Preset` structs (no NaNs, ranges respected).
  - Round-trip: load → APVTS state → re-read → matches.

### 10.3 Layer 2 — Module tests (Automated)

- `yin_detector_test`:
  - Sines at 80, 110, 220, 440, 880, 1760 Hz → detected within ±1 Hz, `voiced=true`, confidence > 0.8.
  - Wobbled sine (220 ± 5 Hz) → tracks within ±2 Hz.
  - Noise / silence → `voiced=false`.
- `pitch_shifter_test`:
  - Render 440 Hz sine through `SignalsmithPitchShifter` at ratios 0.5, 0.75, 1.0, 1.25, 1.5, 2.0 → run `YinPitchDetector` on output → expect target frequency within ±5 Hz.
  - Output peak amplitude within ±2 dB of input (no gain blow-up).
  - Impulse latency probe → ≤ documented latency.
- `voice_test`:
  - ADSR: noteOn → attack rise within attack time; noteOff → release decay within release time.
  - EQ: 1 kHz sine + mid peak +12 dB at 1 kHz → output ~+12 dB.
  - Pan: -1 → only L; +1 → only R.
- `voice_eq_test`: frequency response at known knob settings within tolerance.

### 10.4 Layer 3 — Integration tests (Automated)

`PluginProcessor` instantiated. `processBlock` driven with prepared audio + MIDI. Output compared to small WAVs in `Tests/golden/`.

- `preset_third_up_on_C_major_sine.wav`: dry C4 sine + "Third Up" preset + C major key → E4 harmony rides under dry.
- `midi_chord_on_sine.wav`: dry A3 sine + MIDI C4+E4+G4 held → 3 harmony voices at those pitches.
- `mono_talkbox_on_sine.wav`: dry A3 sine + 1 voice mono + formant on + MIDI legato C4→E4 → single voice tracking C4 then E4 without re-attack.
- `silence_passthrough.wav`: silence in → silence out (no DC, no NaN, no init artifacts).
- `dry_only_when_disabled.wav`: all voices disabled → output == input within float epsilon.

A separate `generate_goldens` target re-renders the goldens. Regeneration is intentional and gated by manual Mac verification.

### 10.5 Layer 4 — Host-API validation / pluginval (Automated)

Pluginval runs as a `ctest` step against the host's native plugin format:
- Windows CI: VST3.
- macOS CI: AU.
- Catches state save/restore violations, processBlock contract violations, threading issues, edge-case sample rates / block sizes.

### 10.6 Layer 5 — Manual on the M1 Mac (Manual — only manual layer)

- Download `.vst3` + `.component` from the latest macOS CI artifact.
- Drop into `~/Library/Audio/Plug-Ins/VST3/` and `~/Library/Audio/Plug-Ins/Components/`.
- Load in Logic (AU) / Reaper (VST3).
- Smoke tests:
  1. Sing/play through each shipped preset.
  2. Talkbox mode with MIDI keyboard.
  3. Latency feel.
  4. Silence, very high/low input, very fast MIDI changes.
- Any surprise reproduces as a Layer 1–3 test before being fixed.

### 10.7 CI

GitHub Actions matrix: `windows-latest` and `macos-latest` (Apple Silicon). Both build the plugin, run all tests, run pluginval on the native format. macOS job uploads `Multivoicer.vst3` and `Multivoicer.component` as workflow artifacts for direct download to the M1.

### 10.8 Out of scope for V1 testing

- Audio "quality" (clarity, naturalness) — ear judgment, intentionally manual.
- CPU/RAM perf gates.
- Host-specific compat tests (we trust JUCE; we'll catch host quirks manually).

## 11. V1 acceptance criteria

V1 is complete when:

1. The plugin builds VST3 + Standalone on Windows CI and VST3 + AU + Standalone on macOS CI.
2. All Layer 0–4 tests pass on both CI runners.
3. macOS CI publishes downloadable `.vst3` and `.component` artifacts.
4. Manual Mac listening sessions confirm each of the 6 default presets produces musically coherent harmony on at least one vocal source and one guitar source.
5. Talkbox mode (preset 6 with MIDI) produces a recognizable vocoder/talkbox effect.
6. Latency at default settings is ≤ 50 ms round-trip.

## 12. Out of V1 scope (explicit non-goals)

- Designed UI / custom look-and-feel.
- Modes beyond major + natural minor.
- DIY phase-vocoder / PSOLA pitch shifter (interface is ready; implementation is a follow-on).
- Glide/portamento.
- MPE / per-channel MIDI routing.
- MIDI CC mappings.
- Disk preset files.
- Latency tightening below 20 ms.
- Polyphonic input (chords on guitar). Input is monophonic by spec.
- Performance benchmarking / SIMD tuning.

## 13. Risks and mitigations

| Risk | Mitigation |
|---|---|
| Signalsmith Stretch latency too high for live monitoring | V1 targets the loose 20–50 ms band. Tightening is a milestone, not a ship gate. |
| YIN detector unreliable on certain vocal sources (whisper, breathy) | Layer 2 tests catch synthetic failures. Real-source failures get added as regression fixtures from manual Mac sessions. |
| 64 APVTS params overwhelm `GenericAudioProcessorEditor` | Accepted V1 cost. Grouped APVTS layout means V1.5 UI work is parameter-stable. |
| Cross-platform build divergence (Windows vs macOS) | CI matrix builds both on every push. |
| Pluginval flagging things we don't understand | Treat first run as a learning gate. Triage issues case-by-case; do not silence them. |
| FetchContent network flakes break CI | Cache FetchContent downloads in the CI runner. |

---
