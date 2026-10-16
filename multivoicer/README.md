# Multivoicer

A JUCE diatonic-harmonizer plugin (VST3 + AU) with preset and MIDI control modes, up to 4 voices, per-voice ADSR and EQ, formant-preserving pitch shifting, and a talkbox-style mono MIDI mode.

See [`docs/superpowers/specs/2026-05-28-multivoicer-design.md`](../docs/superpowers/specs/2026-05-28-multivoicer-design.md) for the full design spec.

## Prerequisites

You need a C++17 compiler, CMake ≥ 3.22, and `git`. Network access is required for the first build (FetchContent pulls JUCE, Signalsmith Stretch, Catch2, and pluginval).

**Windows:**
- Visual Studio 2022 with the **Desktop development with C++** workload. That single workload installs the MSVC compiler, the Windows 11 SDK, and the bundled `C++ CMake tools for Windows`.
- If `cmake --version` doesn't work from a plain PowerShell after install, either use the "Developer PowerShell for VS 2022" shortcut from the Start menu, or invoke CMake by its full path:
  `"C:\Program Files\Microsoft Visual Studio\2022\<edition>\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"`

**macOS:**
- Xcode (Command Line Tools at minimum: `xcode-select --install`).
- CMake: `brew install cmake` (or any other install giving you `cmake` ≥ 3.22 on PATH).

**Linux:** A modern GCC or Clang, CMake, plus the usual ALSA/X11/FreeType dev packages JUCE wants. VST3 + Standalone build; AU is macOS-only.

## Build from a fresh clone

All commands run from the **repo root** (the directory containing this `multivoicer/` folder).

```sh
git clone https://github.com/soxthefox/AudioDevProjects.git
cd AudioDevProjects

cmake -S multivoicer -B multivoicer/build
cmake --build multivoicer/build --config Release
```

The first configure takes a few minutes — it clones JUCE, Signalsmith Stretch, Catch2, and pluginval, then compiles JUCE's `juceaide` helper. Subsequent configures hit the cache and finish in seconds.

The first build also takes a few minutes (all of JUCE compiles once). Incremental builds after that compile only the changed files.

## Plugin artifacts

After a successful Release build everything lands under `multivoicer/build/Multivoicer_artefacts/Release/`:

| Platform | Path | What it is |
|---|---|---|
| Windows | `Standalone/Multivoicer.exe` | Standalone host — double-click to run |
| Windows | `VST3/Multivoicer.vst3/Contents/x86_64-win/Multivoicer.vst3` | VST3 DLL inside a bundle folder. Install the **outer `Multivoicer.vst3` folder**, not the inner DLL. |
| macOS | `Standalone/Multivoicer.app` | Standalone host — double-click to run |
| macOS | `VST3/Multivoicer.vst3` | VST3 bundle |
| macOS | `AU/Multivoicer.component` | Audio Unit bundle |

### Run the standalone (no DAW required)

**Windows:**
```powershell
.\multivoicer\build\Multivoicer_artefacts\Release\Standalone\Multivoicer.exe
```
**macOS:**
```sh
open multivoicer/build/Multivoicer_artefacts/Release/Standalone/Multivoicer.app
```
First launch shows JUCE's Audio/MIDI Settings dialog. Pick your input + output devices, optionally select a MIDI controller, then close it to reach the main parameter panel.

> Note: "Feedback Loop: Mute audio input" is on by default. With it on, you hear only the harmony voices; the dry signal is muted. Uncheck it to hear dry + wet — and wear headphones if your input is a mic on the same machine as the output.

### Install the VST3 / AU for your DAW

**Windows VST3** (system-scoped — needs admin):
```powershell
$dest = "$env:CommonProgramFiles\VST3\Multivoicer.vst3"
if (Test-Path $dest) { Remove-Item -Recurse -Force $dest }
Copy-Item -Recurse "multivoicer\build\Multivoicer_artefacts\Release\VST3\Multivoicer.vst3" $dest
```

**macOS VST3 + AU:**
```sh
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
rm -rf ~/Library/Audio/Plug-Ins/VST3/Multivoicer.vst3 \
       ~/Library/Audio/Plug-Ins/Components/Multivoicer.component
cp -R multivoicer/build/Multivoicer_artefacts/Release/VST3/Multivoicer.vst3 \
      ~/Library/Audio/Plug-Ins/VST3/
cp -R multivoicer/build/Multivoicer_artefacts/Release/AU/Multivoicer.component \
      ~/Library/Audio/Plug-Ins/Components/
```

Restart your DAW to make it re-scan. Logic Pro only sees AU; most other DAWs see both.

## Tests

```sh
ctest --test-dir multivoicer/build -C Release --output-on-failure
```

Runs the full suite: ~40 unit/integration tests plus `pluginval` against the host's native plugin format (VST3 on Windows, VST3 on macOS). The build dependency `MultivoicerTests → Multivoicer` means a broken plugin compile fails the tests too.

## Don't want to build on macOS yourself?

Every push to `main` or `feat/multivoicer-*` triggers a [GitHub Actions matrix CI](../.github/workflows/multivoicer-ci.yml) that builds on both `windows-latest` and `macos-latest` and runs the full test suite. The macOS job uploads `Multivoicer.vst3` and `Multivoicer.component` as a downloadable artifact named `Multivoicer-macOS` — grab that from the run page and drop the bundles into `~/Library/Audio/Plug-Ins/{VST3,Components}/` on your Mac. No local macOS compile required.

## Project layout

```
multivoicer/
  CMakeLists.txt
  cmake/                       FetchContent modules (JUCE, Signalsmith, Catch2, pluginval)
  Source/
    Plugin/                    PluginProcessor, PluginEditor, Parameters (APVTS)
    Music/                     MusicTheory, Scales (key + diatonic interval math)
    Dsp/                       PitchDetector (DIY YIN), PitchShifter (Signalsmith), VoiceEq
    Voicing/                   Voice, VoiceManager, MidiVoiceAllocator, ModeRouter
    Presets/                   Preset, PresetBank (6 defaults)
  Tests/                       Catch2 v3 — one file per module + integration tests
  build/                       gitignored — CMake output, plugin artifacts
```
