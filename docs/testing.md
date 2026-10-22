# Zephyr unit and regression tests

The full regression gate includes the unit suite: a unit test can also protect
against regressions. Tests compile actual application modules against deterministic
platform fixtures; no alternative platform implementation is built.

## Run host checks

```sh
make test-unit
make test-regression
make test
make format-check
```

`make test` equals `make test-regression`, the complete host CI gate. Python 3.9+,
Make and a C++20 compiler with AddressSanitizer/UndefinedBehaviorSanitizer are
required. No board, IDE, download or `make setup` is required for host tests.
Compilation, sanitizer errors, failed assertions and Python failures stop the suite
with nonzero status. `make format-check` additionally needs the pinned optional
formatters described in [source style](source-style.md).

## Unit coverage

| Entry | Actual behavior exercised |
| --- | --- |
| [audio_queue](../tests/audio_queue_test.cpp) | Empty/full, FIFO, wrap, rejected writes without overwrite, incomplete/oversized reads preserving data |
| [fft_power](../tests/fft_power_test.cpp) | DC/Nyquist and interior power, first/last bins, exact input/output bounds |
| [keyword_filter](../tests/keyword_filter_test.cpp) | Qualification, isolated/held suppression, release, cooldown and millis rollover |
| [keyword_detector](../tests/keyword_detector_test.cpp) | Evidence timing, confirmation/reset and micros rollover |
| [keyword_event](../tests/keyword_event_test.cpp) | Generation/age/sequence/cadence rejection, dual-clock rollover and latch/cooldown preservation |
| [timing_stats](../tests/timing_stats_test.cpp) | Empty statistics, average/maximum/last, elapsed/count rollover and average-window rebasing |
| [device_action](../tests/device_action_test.py) | Real LED module with fake GPIO: initial state and alternating confirmed actions |
| [audio_capture](../tests/audio_capture_test.py) | Real receiver with PDM and Zephyr IRQ fixtures: failures, exclusive ownership, bounded/partial reads, overflow, full-ring timestamps, discard/destruction/handoff and nested IRQ-key restoration |
| [feature_extractor](../tests/feature_extractor_test.py) | Real extractor with scalar DSP backend: initialization, priming, cadence, reset, independent contexts, matrix/NaN/infinity/negative-energy rejection, clipping, periodic Hann coefficients and 84,180 frozen quantized bytes |
| [keyword_model](../tests/keyword_model_test.py) | Real adapter with fake TFLite and startup Serial: sixteen process-isolated initialization/metadata cases, input copy, invocation failure and invalid output rejection |
| [telemetry](../tests/telemetry_test.py) | Actual serial pump: bounded startup wait, post-startup report interval, disconnected/zero-space behavior, partial sends, 64-byte budget, no repeated record, reconnect and nonfinite/out-of-range diagnostic scores |
| [buffer_contract](../tests/buffer_contract_test.py) | Eleven compiler probes for valid typed buffers, wrong extents/capacities, raw-pointer reads and noncopyable owners; reject C++11/14/17 |

## Integration and regression coverage

| Entry | Actual behavior exercised |
| --- | --- |
| [stream](../tests/stream_test.cpp) | Ten simulated seconds of ordered 16 kHz samples with 60 ms consumer pauses |
| [pipeline](../tests/pipeline_test.py) | Real audio pipeline with fake dependencies: readiness, backlog deferral, timestamps, timing, failure and loss-triggered reset |
| [build_tooling](../tests/build_tooling_test.py) | Eleven Python cases: checksum/cache/path safety, source/lock integrity, Zephyr core-header restoration, ambient CLI isolation, C++20/PDM/static-FQBN properties, official compiler version/missing installation, safety flags, object-audit rejection, PDM override integrity/context and explicit library selection, staged-source provenance and exclusion of unused dependency examples |
| [zephyr_startup](../tests/zephyr_startup_test.py) | Real coordinator/guards with native `k_*` fixtures: fixed stacks/priorities, model/DSP/capture failure, producer-before-consumer startup, cleanup/idempotence, coalesced semaphore, eight-hop batch/yield, low-priority main diagnostics, blocked-telemetry isolation, failure idle, metrics contention, nested scheduler locks, final-action expiry/generation invalidation and event-overflow recovery |

The suite has twelve unit entries and four additional regression entries. Entries contain
many assertions; suite counts are not assertion counts. Zephyr fixtures represent
the APIs used by the firmware; they are not a complete kernel simulator.

## Pinned driver fault gate

After `make setup`, run `make test-pdm-driver`. This separate board-free gate
needs the installed core source, so the download-free `make test` suite does not
invoke it. The firmware CI job runs it after setup. It compiles the actual
original/staged PDM worker against fixtures with sanitizers: the original must
fail the yield-before-retry check, while the patched worker must yield after
transient and 1,000 persistent read errors. It also exercises successful callback
delivery, queue-full slab release and a callback that makes no progress.

This is host fault injection into the actual worker body, not device-driver
execution on the Nano. [PDM compatibility](../build-support/pdm/README.md) explains
the source patch and its limits. The cross-build and live captures separately
check the full staged library on the board.

## Hardware checks

After `make setup` and `make flash PORT=<port>`, with PySerial installed:

```sh
python3 tests/hardware_capture.py --port /dev/cu.usbmodem11301 --seconds 60 \
    --allow-unavailable-stack-metrics --output .build/normal.json
python3 scripts/rtos_stress.py --port /dev/cu.usbmodem11301 --seconds 25 \
    --output .build/stress.json
```

Normal capture requires advancing audio/inference counts, zero reported sample
losses, missed buffers, DSP/inference/notification errors, score drops and stale
scores, bounded ring occupancy and recent audio. Stock-loader `stack_free=0/0`
is accepted only by the explicit option. Any measured nonzero stack values still
must exceed the 512-byte smoke-test floor. This is not a complete memory budget.

Stress injects a 1500 ms control stall. Nonzero score drops and stale-score
rejections are required while acquisition/inference continue without reported
audio loss or inference errors. Counts depend on scheduling; the test does not
require exactly four/nine. The script rebuilds/uploads normal firmware in
`finally`; power loss or external termination can interrupt restoration, so
check its result. The maintained source never enables the stress define by default.

Speak “yes” with pauses and separately observe LED changes. Reopen the serial
monitor after leaving it closed to check independence from host connection.
A pending telemetry record can precede current output. [Build](build.md) covers
ports, bootloader recovery and local PySerial installation.

## Evidence, CI and limits

[Validation](validation.md) links identified builds, captured records and actual
GitHub runs. CI separately checks source style, the complete host suite and a
clean Zephyr cross-build with artifact upload. It does not exercise hardware.

The scalar DSP backend is not CMSIS-DSP. The model fixture checks integration,
not trained network execution. Kernel/IRQ/PDM fixtures do not reproduce physical
concurrency or every driver fault. Host sanitizers cannot certify all target
accesses. Shared-PCM TensorFlow-versus-firmware feature parity, calibrated model
accuracy, acoustic-onset latency, cold boot, extended endurance and system
heap/stack margins remain separate evidence. The actual arena allocation has
been measured in the dated Zephyr hardware session.

No line/branch coverage report or percentage threshold is currently generated.
The suite inventory describes behavioral coverage, not a measured coverage rate.
There is no exhaustive scheduler-interleaving or ThreadSanitizer test gate; the
native kernel fixtures and physical control-stall test cover selected cases.
