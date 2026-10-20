# Validation — October 20, 2026

This is a historical validation record for the build described below. For the
current command-line build procedure and measured memory use, see the
[build guide](build.md) and [current verification](realtime-review.md#verification).

Changes were implemented in the requested order. Each item's focused host test
passed before implementation of the following item began:

1. Audio buffering: ordering, full-queue rejection without overwrite, wraparound,
   and 10,000 repeated push/pop operations.
2. FFT handling: exact-size 512-float input / 257-bin output under AddressSanitizer
   and UndefinedBehaviorSanitizer; separate DC, Nyquist, and interior complex-bin powers.
3. Continuous processing: ten-second simulated 16 kHz input with repeated 60 ms
   inference stalls, ordered consumption, and no ring overflow.
4. Keyword filtering: isolated spike rejection, sustained-score confirmation,
   hysteresis, suppression while latched, release/rearming, and cooldown.
5. Device control: compile the sketch's actual action function against fake GPIO;
   verify successive calls toggle the configured LED pin high and low.
6. Instrumentation: timing averages/maxima, empty statistics, and unsigned timer
   wraparound. All host tests were rerun successfully after integration.

## Build and upload

Arduino CLI, `arduino:mbed_nano:nano33ble`, installed core 4.6.0,
Arduino_CMSIS-DSP 5.7.0, Arduino_TensorFlowLite 2.4.0-ALPHA.
Build succeeded: 327,928 bytes flash (33%), 194,112 bytes static RAM (74%),
68,032 bytes remaining for stack/heap. The installed TensorFlow library fell
back to source compilation because its precompiled binary was unavailable.

Firmware uploaded successfully to the connected Nano 33 BLE on
`/dev/cu.usbmodem11201`. No external GPIO hardware was required.

## Hardware results

Initial 15-second capture and the subsequent 60-second capture both passed the
hardware checker. The later capture includes one older USB-buffered record;
subsequent records span uptime 540.319–599.674 seconds. Cumulative counters at
the end therefore also cover approximately ten minutes of board operation.

| Measurement | Final recorded value |
|---|---:|
| Received samples | 9562368 |
| Inferences | 4662 |
| Dropped samples | 0 |
| Callbacks with dropped samples | 0 |
| Inference errors | 0 |
| Peak ring occupancy (of 8192) | 768 |
| Largest PDM callback gap (µs) | 16042 |
| DSP average / maximum (µs) | 2460/3489 |
| Inference average / maximum (µs) | 59022/59220 |
| Processing average / maximum (µs) | 9926/61916 |
| Response last / maximum (µs) | 445639/445772 |
| Confirmed LED actions | 26 |

During the user's spoken-word test, telemetry showed new confirmed detections
and alternating LED states. No acquisition stalls or reported queue losses were
observed during inference or those actions. Raw evidence is in
[hardware-telemetry.json](hardware-telemetry.json).

Response timing starts at the estimated end of the first high-confidence input
window, not the start of the spoken word. The ~1-second rolling context and
~128 ms decision cadence also affect perceived response time. A controlled audio
stimulus and independent timestamp are required for acoustic onset-to-action
latency. This session does not establish a false-positive/false-negative rate,
or prove absence of losses inside the PDM hardware/driver; sample progression
and callback-gap measurements complement the ring loss counters.

Reproduce host checks with `sh tests/run_tests.sh`. With pyserial installed,
run `python3 tests/hardware_capture.py --port /dev/cu.usbmodem11201 --seconds 60`
for acquisition/inference checks. Repeat spoken `yes` attempts with pauses and
observe the built-in LED to check behavior in the deployment environment.


## Subsequent module refactor

The application was later separated into capture, features, model, detection,
control, telemetry, and coordinator modules. The original FFT/buffering fixes
and detection settings remain in place. See
[module separation validation](module-refactor-validation.md) for the module-refactor
build and repeated spoken-word/physical LED results, and the
[documentation index](README.md) for all raw captures.
