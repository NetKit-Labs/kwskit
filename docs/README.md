# Project documentation

The current firmware architecture, setup, filtering configuration, and telemetry
fields are documented in the [project README](../README.md).
For current scheduling and API use, see [RTOS design](rtos-design.md), including
[why two tasks are used](rtos-design.md#why-two-application-tasks) and
[the API source map](rtos-design.md#rtos-api-source-map). Latest build and hardware
measurements are in the [C++ idioms verification](cpp-idioms.md#verification).
Earlier validation records and captures retain their original results as history.

| Record | Contents |
|---|---|
| [C++ idioms normal telemetry](cpp-idioms-normal.json) | One-minute normal-operation regression and developer-confirmed spoken-word LED check |
| [C++ idioms stress telemetry](cpp-idioms-stress.json) | Live control-stall regression after typed interfaces and scoped ownership changes |
| [C++ idioms and ownership review](cpp-idioms.md) | Typed buffer/FFT contracts, RAII critical sections and resource lifetimes, source audit, and regression evidence |
| [Engineering overview](engineering-overview.md) | Requirements, design tradeoffs, source entry points, measured margins, and claim-to-evidence mapping |
| [C++ ownership and refactor](cpp-refactor.md) | Object lifetimes, explicit dependencies, fixed storage, callback boundaries, and refactor regression/hardware evidence |
| [Periodic Hann normal telemetry](periodic-hann-normal.json) | One-minute continuous audio/inference and spoken-word check after loading the updated normal firmware |
| [Periodic Hann stress telemetry](periodic-hann-stress.json) | Control-stall acquisition/inference regression using the periodic-window firmware |
| [Periodic Hann validation](periodic-hann-validation.md) | Training-aligned window convention, coefficient checks, feature regression, build and Nano verification |
| [DSP pipeline](dsp-pipeline.md) | PCM scaling, frame overlap, Hann/packed FFT power, mel/log features, int8 layout, cadence/reset, workspace, and training alignment |
| [TinyML inference](tinyml-inference.md) | CNN layers, training/deployed parameter counts, int8 quantization, model identity, flash/SRAM placement, runtime flow, and inspection commands |
| [Model metadata](model-metadata.json) | Inspected deployed operator graph, tensor shapes/types/quantization, parameter counts, and constant payload sizes |
| [Unit and regression testing](testing.md) | Test commands, component/integration coverage, CI regression gate, hardware procedures, and limits |
| [Source language and style](source-style.md) | C++ source audit, four-space formatting rules, formatter setup/CI checks, and validation |
| [RTOS design](rtos-design.md) | Two-task rationale, interrupt-driven capture, priorities, static memory, synchronization, API source map, startup, overload rules |
| [RTOS validation](rtos-validation.md) | Current verification summary and historical task-introduction, scheduling, backpressure, and speech evidence |
| [C++ refactor normal telemetry](cpp-refactor-normal.json) | One-minute acquisition/inference check after restoring the object-based normal firmware |
| [C++ refactor stress telemetry](cpp-refactor-stress.json) | Repeated control-stall regression on the object-based firmware |
| [RTOS spoken "yes" telemetry](rtos-spoken-yes.json) | Raw speech detections and LED transitions from the original RTOS implementation |
| [RTOS stress telemetry](rtos-stress-telemetry.json) | Raw deliberate control-stall and score-queue overflow results |
| [RTOS-introduction normal telemetry](rtos-final-normal.json) | Historical normal operation after the latch-preservation fix and stress restoration |
| [RTOS-introduction stress repeat](rtos-stress-final.json) | Historical repeated 1.5 s control stall: expected score drops/stale rejection, zero reported audio loss |
| [Locked-build validation](build-validation.md) | Clean setup, Make targets, host checks, upload and live telemetry validation |
| [Build and CI](build.md) | Locked command-line tools, dependency provenance, Make targets, CI artifacts, and reproducibility limits |
| [Initial improvements validation](validation.md) | Ordered testing of the six improvements, Nano build/upload, and initial hardware measurements |
| [Module separation validation](module-refactor-validation.md) | Module boundaries, host regression tests, memory use for that historical build, 30-second live check, and repeated one-minute speech/physical LED test |
| [Locked CLI build telemetry](cli-build-telemetry.json) | Raw records after uploading through the documented Make workflow |
| [Initial hardware telemetry](hardware-telemetry.json) | Raw records from the initial implementation |
| [Refactor telemetry](module-refactor-telemetry.json) | Raw records from the refactored acquisition/inference check |
| [Refactor spoken “yes” telemetry](module-refactor-spoken-yes.json) | Raw records showing confirmed detections and alternating LED states during the repeated speech test |

The JSON captures contain cumulative firmware counters at each timestamp.
Compare successive records to measure activity within a captured interval;
final cumulative values can include operation before recording began.
Response timing starts at the estimated end of the first high-confidence audio
window, rather than at acoustic word onset.

[README.pdf](README.pdf) is the current PDF export of the project overview.
The [build guide](build.md) documents how to regenerate it from the project README. The model diagram remains available as
[kws_1class_depthwise_int8.tflite.svg](kws_1class_depthwise_int8.tflite.svg).

## Reproducing checks

From the repository root, run `make test` with a C++ compiler and
Python 3.9+. `make test-unit` selects isolated component checks;
`make test-regression` runs the full suite, including unit tests. See the
[testing guide](testing.md) for coverage. Run `make setup` then `make build`
for a project-local firmware build.
The [build guide](build.md) includes cloning, prerequisites, flashing, serial
verification, and troubleshooting. For hardware checks, install pyserial and run
`python3 tests/hardware_capture.py --port <Nano-serial-port> --seconds 60`.
Speak “yes” with pauses of about three seconds and observe the built-in LED.
The capture script checks acquisition/inference progression and loss/error
counters. Verify speech-trigger counts and physical LED changes separately.
