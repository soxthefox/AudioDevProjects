# Decisions

Departures from, or clarifications of, `HANDOVER.md`. Newest last. Each entry says what was decided and why.

## D1. Project lives in `stinger/`, not the repository root

The repository (`AudioDevProjects`) is a monorepo whose convention is one self-contained project per top-level folder. The handover's "repo root" layout (`protocol/`, `firmware/`, …) is therefore placed under `stinger/`, and `HANDOVER.md` sits at `stinger/HANDOVER.md`. All paths in the handover are relative to `stinger/`.

## D2. Python 3.14 from Homebrew in a project virtualenv

The plan asks for Python 3.11+. The machine's default `python3` is 3.9, so `make` creates `stinger/.venv` from the newest `python3.1x` it finds (3.14 here) and installs dev tools there, including a current PlatformIO. Nothing is installed system-wide.
