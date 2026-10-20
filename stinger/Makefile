# One entry point for everything testable. Nothing here flashes boards or opens serial ports.

PYTHON_FOR_VENV ?= $(firstword $(foreach v,3.14 3.13 3.12 3.11,$(shell command -v python$(v))))
VENV := .venv
BIN := $(VENV)/bin
PIO := $(BIN)/pio
CLANG_FORMAT ?= $(shell command -v clang-format || xcrun --find clang-format 2>/dev/null)

FIRMWARE_PROJECTS := firmware/gateway
# Generated files are excluded from formatting.
C_SOURCES := $(shell find protocol firmware -name '*.[ch]' -o -name '*.cpp' -o -name '*.hpp' \
	| grep -v -e '/\.pio/' -e 'golden_vectors.h')

.PHONY: help venv test lint format py-test native-test firmware-build vectors clean

help:
	@echo "make test            lint + Python tests + PlatformIO native tests + firmware builds"
	@echo "make lint            ruff + clang-format checks"
	@echo "make format          apply ruff format and clang-format"
	@echo "make vectors         regenerate protocol/vectors/golden_vectors.h"

$(BIN)/python:
	@test -n "$(PYTHON_FOR_VENV)" || (echo "need Python 3.11+ (python3.11 ... python3.14) on PATH" && exit 1)
	$(PYTHON_FOR_VENV) -m venv $(VENV)
	$(BIN)/pip install -q --upgrade pip

$(VENV)/.installed: pyproject.toml | $(BIN)/python
	$(BIN)/pip install -q -e '.[dev]'
	@touch $@

venv: $(VENV)/.installed

test: lint py-test native-test firmware-build

lint: venv
	$(BIN)/ruff check .
	$(BIN)/ruff format --check .
	$(CLANG_FORMAT) --dry-run --Werror $(C_SOURCES)

format: venv
	$(BIN)/ruff format .
	$(BIN)/ruff check --fix .
	$(CLANG_FORMAT) -i $(C_SOURCES)

py-test: venv
	$(BIN)/pytest

native-test: venv
	@for p in $(FIRMWARE_PROJECTS); do \
		echo "== native tests: $$p"; \
		(cd $$p && ../../$(PIO) test -e native) || exit 1; \
	done

firmware-build: venv

vectors: venv
	$(BIN)/python protocol/vectors/gen_c_vectors.py

clean:
	rm -rf $(VENV) $(addsuffix /.pio,$(FIRMWARE_PROJECTS))
