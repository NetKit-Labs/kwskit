# Project documentation

The current firmware architecture, setup, filtering configuration, and telemetry
fields are documented in the [project README](../README.md).

| Record | Contents |
|---|---|
| [Initial improvements validation](validation.md) | Ordered testing of the six improvements, Nano build/upload, and initial hardware measurements |
| [Module separation validation](module-refactor-validation.md) | Module boundaries, host regression tests, current memory use, 30-second live check, and repeated one-minute speech/physical LED test |
| [Initial hardware telemetry](hardware-telemetry.json) | Raw records from the initial implementation |
| [Refactor telemetry](module-refactor-telemetry.json) | Raw records from the refactored acquisition/inference check |
| [Refactor spoken “yes” telemetry](module-refactor-spoken-yes.json) | Raw records showing confirmed detections and alternating LED states during the repeated speech test |

The JSON captures contain cumulative firmware counters at each timestamp.
Compare successive records to measure activity within a captured interval;
final cumulative values can include operation before recording began.
Response timing starts at the estimated end of the first high-confidence audio
window, rather than at acoustic word onset.

`README.pdf` is the original overview and predates the firmware improvements and
module refactor. Use the Markdown documents above for the current implementation
and validation status. The model diagram remains available as
[kws_1class_depthwise_int8.tflite.svg](kws_1class_depthwise_int8.tflite.svg).

## Reproducing checks

From the repository root, run `sh tests/run_tests.sh` with a C++ compiler and
Python 3. For hardware checks, install pyserial and run
`python3 tests/hardware_capture.py --port <Nano-serial-port> --seconds 60`.
Speak “yes” with pauses of about three seconds and observe the built-in LED.
The capture script checks acquisition/inference progression and loss/error
counters. Verify speech-trigger counts and physical LED changes separately.
