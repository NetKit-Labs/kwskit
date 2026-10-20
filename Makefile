PYTHON ?= python3
JOBS ?= 2
PORT ?=

.DEFAULT_GOAL := help
.PHONY: help setup build test flash boards

help:
	@echo 'make setup                 Install locked tools/libraries into .build/'
	@echo 'make build [JOBS=2]         Compile firmware and write .build/artifacts/'
	@echo 'make test                  Run host unit/integration tests with sanitizers'
	@echo 'make boards                List connected boards'
	@echo 'make flash PORT=<port>     Build and upload to the selected Nano'

setup:
	$(PYTHON) scripts/project.py setup

build:
	$(PYTHON) scripts/project.py build --jobs '$(JOBS)'

test:
	sh tests/run_tests.sh

boards:
	$(PYTHON) scripts/project.py boards

flash:
	$(PYTHON) scripts/project.py flash --jobs '$(JOBS)' --port '$(PORT)'
