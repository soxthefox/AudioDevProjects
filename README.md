# AudioDevProjects

A collection of audio development projects — plugins, DSP experiments, and audio apps. Each project lives in its own top-level folder and is self-contained (own build system, own dependencies, own tests).

## Structure

```
AudioDevProjects/
  <project-name>/
    README.md          how to build, run, test
    CMakeLists.txt     (or Cargo.toml, package.json, ...)
    Source/            (or src/)
    Tests/
  ...
  .gitignore           root-level ignores (build outputs, IDE/OS noise)
  README.md
```

Projects do not share code at the repo level. If a useful helper emerges across two projects, copy it; only promote to a shared library once a third project needs it.

## Conventions

- **Default plugin stack:** [JUCE](https://juce.com/) (C++ / CMake), targeting VST3 + AU.
- **Not every project is a plugin.** Standalone audio apps, DSP experiments, and scripts are welcome — pick whatever stack fits the project.
- **Per-project README** documents how to build and run that specific project.

## Platforms

- **Development:** Windows (primary editing/building).
- **Manual audio testing:** macOS on Apple Silicon (M1). All plugin/audio projects must build and run on macOS arm64.
- **Automated tests** should be runnable headlessly on either OS where practical.

## Adding a new project

1. Create a new folder at the repo root with a clear name.
2. Add a `README.md` describing what it is and how to build/test it.
3. Add a local `.gitignore` if it needs language-specific ignores beyond the root.
