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
