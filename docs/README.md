# Project documentation

The current firmware architecture, setup, filtering configuration, and telemetry
fields are documented in the [project README](../README.md).

| Record | Contents |
|---|---|
| [RTOS design](rtos-design.md) | Audio/control task ownership, priorities, static memory, ISR notifications, event overload rules |
| [RTOS validation](rtos-validation.md) | Current host, build, live scheduling, backpressure, and speech test evidence |
| [RTOS spoken "yes" telemetry](rtos-spoken-yes.json) | Raw current-task speech detections and LED transitions |
| [RTOS stress telemetry](rtos-stress-telemetry.json) | Raw deliberate control-stall and score-queue overflow results |
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
Python 3.9+. Run `make setup` then `make build` for a project-local firmware build.
The [build guide](build.md) includes cloning, prerequisites, flashing, serial
verification, and troubleshooting. For hardware checks, install pyserial and run
`python3 tests/hardware_capture.py --port <Nano-serial-port> --seconds 60`.
Speak “yes” with pauses of about three seconds and observe the built-in LED.
The capture script checks acquisition/inference progression and loss/error
counters. Verify speech-trigger counts and physical LED changes separately.
