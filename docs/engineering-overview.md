# Engineering overview: real-time keyword spotting on Cortex-M4F

This project implements continuous, single-keyword voice control on an nRF52840
at 64 MHz using an Arduino Nano 33 BLE-class board with a PDM microphone. CMSIS-DSP
extracts log-mel features, an int8 TensorFlow Lite Micro model scores the keyword
"yes," and confirmed detections toggle the built-in LED. Two CMSIS-RTOS2 application
tasks run on the board's existing Mbed/RTX kernel.

The implementation addresses a concrete scheduling problem: microphone samples
continue arriving every 16 ms while a neural-network inference takes roughly
59 ms. Acquisition must preserve sample order during inference, and delayed
device-control work must not block the audio consumer. Detection also needs
fresh temporal evidence to avoid acting on old scores after a stall.

## Requirements and design decisions

| Requirement | Implementation and tradeoff |
|---|---|
| Preserve audio during bounded processing delays | PDM interrupt callback publishes into an 8192-sample SPSC ring. The ring provides 512 ms nominal capacity and rejects new samples when full rather than overwriting unread data. |
| Keep interrupt work bounded to acquisition | Callback drains PDM data, updates counters, and sets an ISR-safe thread flag. DSP, inference, filtering, GPIO actions, and USB telemetry execute in task context. |
| Separate processing from device response | Higher-priority audio task owns DSP/model state; normal-priority control task owns filtering, LED state, and telemetry. Preemption occurs on one CPU; tasks are not executing simultaneously. |
| Avoid pointer-lifetime and ownership ambiguity | An eight-entry RTOS queue copies score/timestamp/sequence/generation values. No audio, feature, or tensor pointers cross tasks. |
| Bound control backpressure | Queue sends never wait. Overflow drops a score and invalidates previous-generation evidence, allowing audio to continue at the cost of possibly missing commands during the stall. |
| Reject obsolete or incomplete detection evidence | Control rejects scores older than 250 ms or from old generations. Missing/duplicate sequences and long score gaps reset temporal evidence while preserving latch/cooldown history. |
| Suppress isolated/repeated detections | EMA smoothing, raw/smoothed confidence thresholds, two qualifying decisions, release hysteresis, and a 1500 ms cooldown precede an LED action. |
| Make builds and failures reviewable | Pinned dependencies, source/dependency manifests, host regression tests, hardware telemetry, checked initialization, and cleanup on partial RTOS startup failure. |

## Runtime and source navigation

The [README architecture diagram](../README.md#runtime-architecture-and-ownership)
shows the complete flow. The main ownership boundary is:

```text
PDM interrupt -> sample ring -> audio task -> score queue -> control task -> LED
                                  DSP + NN                   filter + USB
```

Start with these source files to follow that flow:

| Source | What to review |
|---|---|
| [kws_pipeline.cpp](../firmware/kws_pipeline.cpp) | Static task/queue storage, priorities, ISR notification, startup/cleanup, nonblocking score transport, and generation handling |
| [audio_capture.cpp](../firmware/audio_capture.cpp), [audio_queue.h](../firmware/audio_queue.h) | PDM staging, atomic SPSC publication, complete-hop reads, and explicit loss counters |
| [audio_pipeline.cpp](../firmware/audio_pipeline.cpp) | Loss recovery, feature readiness, backlog-aware inference, and timing boundaries |
| [feature_extractor.cpp](../firmware/feature_extractor.cpp), [fft_power.h](../firmware/fft_power.h) | Overlapped frames, packed real-FFT power, log-mel features, quantization, and rolling model context |
| [keyword_model.cpp](../firmware/keyword_model.cpp) | Model/operator setup, static 96 KiB arena, tensor validation, and int8 score conversion |
| [keyword_event.h](../firmware/keyword_event.h), [keyword_filter.h](../firmware/keyword_filter.h) | Freshness, sequence continuity, smoothing, hysteresis, and retrigger suppression |
| [device_control.cpp](../firmware/device_control.cpp), [telemetry.cpp](../firmware/telemetry.cpp) | Confirmed-action handling and nonblocking diagnostic output |

Synchronization stays narrow: the ISR never acquires a mutex. A
priority-inheriting mutex protects only a small audio-statistics copy and is not
held across DSP, inference, GPIO, USB transmission, or queue waits. Thread flags
may coalesce because sample ownership lives in the ring; audio drains complete
hops before waiting again. See [RTOS design](rtos-design.md) for the full rules.

The FFT helper explicitly handles the CMSIS fast real-FFT packing: DC is at
output index 0, Nyquist at index 1, and interior complex bins occupy the remaining
pairs. The 512-float transform produces 257 power bins without reading outside
the transform output. Exact-size-array sanitizer tests exercise this boundary.
The [DSP pipeline guide](dsp-pipeline.md) follows every stage through mel/log
projection, quantization, and rolling model context, including startup/reset
behavior and the periodic Hann convention used by training.
The [TinyML inference guide](tinyml-inference.md) describes the deployed CNN,
distinguishes Keras and folded parameter counts, and verifies the model/arena
flash/SRAM placements using the artifact and linked firmware.

## C++ state ownership

The [object refactor](cpp-refactor.md) makes the stateful modules ordinary C++
classes. `FeatureExtractor` owns `std::array` workspaces and is noncopyable because
CMSIS descriptors point into them. `AudioPipeline` receives its dependencies by
reference. `AudioCapture` routes the driver's C callback to its active owner;
RTOS callbacks use an application context pointer. The model has one encapsulated,
statically reserved runtime. DSP/model state remains exclusive to the audio task.

## Measured results and their meaning

Hardware records below were collected October 20, 2026 before the C++ object refactor. The final normal capture
reported the following cumulative timing statistics; the other rows identify
their separate evidence sources. Raw JSON files and test chronology are in
[RTOS validation](rtos-validation.md). Current-build measurements and repeated
checks are in [C++ refactor validation](cpp-refactor.md#verification).

| Measurement | Recorded result / interpretation |
|---|---|
| Input / features | 16 kHz mono; 256-sample hop; 512-sample FFT; 61 x 20 int8 feature context |
| Nominal decisions | Every eight new feature hops, about 128 ms after warmup, when backlog permits |
| Final normal DSP average / observed maximum | 2.464 / 2.637 ms per hop |
| Final normal inference average / observed maximum | 58.630 / 59.652 ms |
| Final normal processing-step observed maximum | 62.234 ms; excludes score dispatch, metrics publication, and control work |
| Final normal result delivery average / observed maximum | 11.002 / 11.586 ms, from inference completion to control-side validation |
| Speech/LED check | 11 confirmed actions; physical LED toggling observed during the 60 s host recording |
| Forced control stall | 1500 ms delay; four score drops and nine stale-result rejections; zero reported audio losses |
| Normal operation | Zero reported audio losses, inference/notification errors, score drops, or stale-result rejections |
| Firmware footprint | 331,440 bytes flash; 209,320 bytes static RAM, including application stacks and tensor arena |

Separately, the developer reports approximately **95% keyword-recognition
success in informal use** with the current **1075 deployed parameter values**.
No labeled trial count or false-trigger rate accompanies that estimate. A wider
or deeper model reaching the upper 90% range is an **untested hypothesis** that
requires suitable training and comparison on shared held-out data, plus timing
and RAM checks. See [recognition evidence and hypothesis](tinyml-inference.md#informal-recognition-and-possible-model-expansion).

With the final normal averages, eight feature hops plus one inference account
for approximately `8 x 2.464 + 58.630 = 78.342 ms` of a nominal 128 ms decision
interval. The difference is about 49.7 ms before other work. This is a useful
processing-budget estimate, not measured total CPU utilization or a worst-case
deadline guarantee. The hardware captures also verify continuing sample and
inference progression rather than relying on this estimate alone.

The deliberate control stall exceeds the score queue's roughly one-second
nominal capacity. Score drops and stale rejection are expected protective
behavior in that experiment, while the audio task continues. A larger event
queue would not make results older than the 250 ms freshness limit usable.
Score loss during routine expected operation would require investigation.

Measured unused stack watermarks were 6784 of 8192 bytes for audio and 4984 of
6144 bytes for control. The ring peaked at 768 of 8192 samples. These exercised
paths leave about 83%, 81%, and 91% capacity respectively. The remaining 52,824
bytes outside static RAM allocation are not a measured free-heap value.
Peak heap, framework/interrupt stacks, and unused tensor-arena capacity have not
been quantified. See the [memory assessment](rtos-validation.md#measured-memory-margins).

## Evidence for reviewers

| Engineering claim | Check / supporting artifact |
|---|---|
| Ring preserves ordering and handles overflow explicitly | [Queue tests](../tests/audio_queue_test.cpp) and [stream-stall simulation](../tests/stream_test.cpp) |
| Packed FFT reads stay within exact buffer bounds | [FFT tests](../tests/fft_power_test.cpp), run with address/undefined-behavior sanitizers |
| Real pipeline defers inference under backlog and resets after loss | [Pipeline integration test](../tests/pipeline_test.py), compiling the actual pipeline with fake dependencies |
| Old/gapped scores do not erase retrigger protection | [Event tests](../tests/keyword_event_test.cpp), [filter tests](../tests/keyword_filter_test.cpp), and [detector tests](../tests/keyword_detector_test.cpp) |
| Task storage/priorities and partial-startup cleanup are checked | [RTOS startup tests](../tests/rtos_startup_test.py), compiling the actual coordinator against fake CMSIS APIs |
| Audio continues while control is deliberately stalled | [Final hardware stress capture](rtos-stress-final.json) and [stress procedure](rtos-design.md#hardware-backpressure-test) |
| Normal firmware is restored and runs without reported losses | [Final normal capture](rtos-final-normal.json) |
| Detection reaches a physical output | [Spoken-word capture](rtos-spoken-yes.json) and physical LED confirmation recorded in [validation](rtos-validation.md#spoken-word-and-physical-led-check) |
| A clean checkout can test/build without IDE-installed libraries | [Make/CI workflow](../.github/workflows/ci.yml), [build-tool tests](../tests/build_tooling_test.py), and [build guide](build.md) |

Run `make test-unit` for the ten component checks, or `make test-regression`
for all fourteen checks including integration/build regressions. `make test` is the
same complete gate used in CI. The [testing guide](testing.md) details coverage
and hardware reproduction.

Host fakes test logic and failure paths; physical captures test actual scheduling
and I/O. Neither substitutes for the other. Saved counters are cumulative since
boot and can include time before capture; trigger counts are not accuracy rates.
The spoken-word capture preceded a small latch-preservation correction; the
final code subsequently passed host regressions and repeated stress/normal
checks, as recorded in the validation chronology.

## Build boundary and remaining validation

The primary workflow is `make setup`, `make test`, `make build`, and
`make flash PORT=<port>`; the [build guide](build.md) covers prerequisites and
reproduction. Make invokes a pinned Arduino CLI to configure the Arm compiler,
board startup/linking, Mbed core, and drivers. The maintained `.ino` only delegates
startup and idle handling; application logic is C++. Arduino IDE is optional,
but the firmware still uses the Arduino Mbed platform. This project does not
claim an independent board-support package or custom RTOS kernel.
All maintained native application/test sources are C++; host tests use C++11 and
the pinned board core selects GNU++14 for firmware. The
[source-style guide](source-style.md) documents the four-space formatter settings,
separate C++ test fixtures, upstream dependency boundary, and CI formatting gate.

Remaining validation includes labeled false-accept/false-reject measurements
across speakers/noise/distance, acoustic-onset response timing, hours-to-days
endurance, the unmeasured memory allocations above, and peripheral-fault recovery.
The observed timing maxima are not established worst-case execution times.
Future communications or device actions must preserve the bounded audio/control
handoff and be evaluated against the timing and memory budgets again.
