# Module separation validation — October 20, 2026

This is a historical validation record for the build described below. For the
current command-line build procedure and measured memory use, see the
[build guide](build.md) and [current verification](cpp17-modernization.md#verification).

The firmware now separates capture, feature extraction, TFLite inference,
detection state, device control, telemetry, and pipeline coordination. Shared
settings are in `kws_config.h`. Stateful implementation details live inside
module-private namespaces; detection state is a hardware-independent class.

The processing cadence, FFT correction, 8192-sample queue, thresholds,
hysteresis, cooldown, and LED toggle behavior are retained. Startup now reports
model/DSP initialization failures and validates int8 tensor sizes and scales
before starting capture.

Host validation (`sh tests/run_tests.sh`) passes:

- Existing queue, packed FFT, stream-stall simulation, confidence filtering,
  and timing-statistics tests under address/undefined-behavior sanitizers.
- New detector tests for confirmation, suppression, evidence timing, reset,
  and microsecond timer wraparound.
- Device-control test compiles the actual extracted module with fake GPIO,
  checking initial state and successive actions.
- Coordinator integration test compiles the actual `kws_pipeline.cpp` against
  deterministic fake modules. It checks initialization failures, no-audio and
  not-ready states, inference deferral during backlog, confirmed actions,
  inference error reset, overflow reset, and telemetry/measurement routing.

The application still runs through `setup()`/`loop()` with interrupt-driven PDM
capture. No application threads or RTOS tasks were introduced. Future task
ownership and snapshot lifetimes are described in the README.


## Nano build and live regression check

The refactored sketch compiled with `arduino:mbed_nano:nano33ble`, core 4.6.0,
CMSIS-DSP 5.7.0 and Arduino_TensorFlowLite 2.4.0-ALPHA, and was uploaded to the
connected Nano 33 BLE. Flash use is 328,304 bytes (33%), 376 bytes above the
previous build. Static RAM remains 194,112 bytes (74%), leaving 68,032 bytes.

A 30-second live telemetry capture passed the hardware checker. A buffered
startup record precedes the regular live records. Final cumulative values:

| Measurement | Value |
|---|---:|
| Received samples | 563456 |
| Inferences | 268 |
| Dropped samples | 0 |
| Missed buffers | 0 |
| Inference errors | 0 |
| Maximum queued samples / 8192 | 768 |
| DSP average / maximum (µs) | 2480/3041 |
| Inference average / maximum (µs) | 59093/59548 |
| Processing average / maximum (µs) | 9789/62870 |

Raw readings: [module-refactor-telemetry.json](module-refactor-telemetry.json).
This initial regression run verifies continuous capture and inference. A subsequent
spoken-word and physical LED check is recorded below. Earlier hardware results
remain in `validation.md`.


## Repeated spoken “yes” test after refactoring

A fresh 60-second hardware capture recorded 12 confirmed detections during the
user's spoken “yes” attempts. Telemetry showed alternating LED states, and the
user explicitly confirmed that the physical built-in LED toggled.

Across the recorded interval (uptime 169.469–228.849 seconds), the inference
counter advanced from 1301 to 1765. All readings reported zero dropped samples,
zero missed buffers, and zero inference errors. Peak queue use was 768 of 8192
samples. Final average/maximum inference execution was 59,093/59,548 µs.
Recorded high-confidence-window-to-action delays ranged from about 190 to
446 ms; this is not an acoustic word-onset latency measurement.

Raw evidence: [module-refactor-spoken-yes.json](module-refactor-spoken-yes.json).
This confirms the refactored application's speech-to-LED path in this session;
it does not measure detection accuracy across a controlled speech/noise dataset.


The later [RTOS design](rtos-design.md) and [RTOS validation](rtos-validation.md)
supersede this record for current task ownership, memory, and runtime results.
