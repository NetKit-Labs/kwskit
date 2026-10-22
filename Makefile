PYTHON ?= python3
JOBS ?= 2
PORT ?=
CLANG_FORMAT ?= .build/style-venv/bin/clang-format
BLACK ?= .build/style-venv/bin/black
CPP_SOURCES := $(wildcard firmware/*.cpp firmware/*.h firmware/*.ino tests/*.cpp tests/fixtures/*/*.cpp tests/fixtures/*/*.h tests/fixtures/*/zephyr/*.h scripts/*.cpp build-support/tflm-compat/*.h)

.DEFAULT_GOAL := help
.PHONY: help setup build test test-unit test-regression format format-check flash boards

help:
	@echo 'make setup                 Install locked tools/libraries into .build/'
	@echo 'make build [JOBS=2]         Compile Zephyr firmware into .build/zephyr-artifacts/'
	@echo 'make test                  Run the complete host regression suite'
	@echo 'make test-unit             Run isolated component unit tests'
	@echo 'make test-regression       Run unit tests plus integration/regression checks'
	@echo 'make format                Apply the maintained four-space code style'
	@echo 'make format-check          Check C++/Python/notebook formatting'
	@echo 'make boards                List connected boards'
	@echo 'make flash PORT=<port>     Build and upload to the selected Nano'

setup:
	$(PYTHON) scripts/project.py setup

build:
	$(PYTHON) scripts/project.py build --jobs '$(JOBS)'

test: test-regression

test-unit:
	sh tests/run_tests.sh unit

test-regression:
	sh tests/run_tests.sh all

format:
	$(CLANG_FORMAT) -i $(CPP_SOURCES)
	$(BLACK) scripts tests training/kws_1class_depthwise.ipynb

format-check:
	$(CLANG_FORMAT) --dry-run --Werror $(CPP_SOURCES)
	$(BLACK) --check scripts tests training/kws_1class_depthwise.ipynb

boards:
	$(PYTHON) scripts/project.py boards

flash:
	$(PYTHON) scripts/project.py flash --jobs '$(JOBS)' --port '$(PORT)'
