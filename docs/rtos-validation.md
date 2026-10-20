# Two-task RTOS validation - October 20, 2026

The firmware uses explicit audio and control application tasks on the Nano's
existing CMSIS-RTOS2/RTX kernel. The PDM callback remains interrupt driven and
runs outside both tasks. See [RTOS design](rtos-design.md) for the rationale,
storage, priorities, synchronization, and API source map.

## Current verification

The latest source and hardware verification is the [real-time review](realtime-review.md#verification),
after the ownership refactors and periodic Hann update. It records fifteen passing
host suite entries, formatter checks, Nano build, normal capture, control-stall
stress, and developer-confirmed physical LED toggling during spoken "yes".

| Current-source measurement | Result |
|---|---|
| Flash / static RAM | 320,088 / 188,392 bytes |
| Outside static allocations | 73,752 bytes; not measured free heap |
| Normal DSP average / observed maximum | 1.987 / 2.129 ms |
| Normal inference average / observed maximum | 57.655 / 57.772 ms |
| Normal processing-step observed maximum | 59.851 ms |
| Minimum observed unused audio / control stack | 6872 / 4680 bytes |
| Peak queued audio / capacity | 768 / 8192 samples |
| Normal reported audio losses, inference/notification errors, score drops, stale results | All zero |
| Deliberate 1.5 s control stall | Four score drops, nine stale rejections, zero reported audio losses |
| Spoken-word output check | 10 cumulative confirmed actions; physical LED toggling confirmed |

Raw current-source captures: [normal](realtime-normal.json) and
[stress](realtime-stress.json). Timing and counters are cumulative snapshots;
the action count is not a labeled accuracy measurement. The memory assessment
limits below also apply to this build.

The remaining sections preserve the original RTOS introduction and its follow-up
regressions. Their footprints, counts, timings, and statements about which
physical checks had occurred describe those earlier builds, not the latest image.

## Host checks

The full `make test` suite passed. Existing queue, packed FFT, stream-stall,
filter, detector, GPIO, timing-statistics, and build-integrity checks are retained.
The actual audio pipeline is tested with fake acquisition/features/model modules
for readiness, backlog deferral, inference failure, and audio-loss reset. New
score-event tests exercise sequence gaps, duplicate scores, stale generations,
old timestamps, timer wraparound, and preservation of cooldown/latch history.
The actual RTOS coordinator is compiled against fake CMSIS startup APIs to check
priorities, static stacks/storage, callback notification, idle behavior, and
cleanup on model/DSP/PDM/task/queue/mutex initialization failures. These host
fakes do not substitute for the physical RTOS scheduling checks below.

## Build and initial live check

`make build JOBS=4` and `make flash PORT=/dev/cu.usbmodem11201 JOBS=4` succeeded.
The normal image uses 331,440 bytes flash (33%) and 209,320 bytes static RAM (79%),
leaving 52,824 bytes outside statically reserved data. The dedicated 8 KiB audio
and 6 KiB control stacks are included in static RAM; this is not a measurement
of total runtime heap availability.

A 15-second capture passed with zero reported audio losses, event drops,
stale events, notification errors, or inference errors. Cumulative inference
count reached 279. Minimum observed unused task stack was 6784 bytes for audio
and 5032 bytes for control. These are measured margins, not a proof of every
possible future stack path.

Raw evidence: [rtos-initial-telemetry.json](rtos-initial-telemetry.json).

## Deliberate control-task backpressure

`scripts/rtos_stress.py` built/uploaded a test-only image that slept the control
task for 1500 ms on its first inference score. A 20-second hardware capture
passed. Audio acquisition and inference continued while control was blocked;
no reported audio samples/buffers were lost and no inference/notification errors
occurred. The score queue overflowed four times, and nine old queued results were
discarded. The maximum recorded result-transport delay was 1,546,437 us, proving
that the blocked consumer path was exercised. No LED actions occurred in this
quiet test. The test then rebuilt and uploaded normal firmware successfully.

Raw evidence: [rtos-stress-telemetry.json](rtos-stress-telemetry.json).
Test reproduction and the automatic normal-firmware restoration are documented
in [RTOS design](rtos-design.md).


## Spoken-word and physical LED check

Normal firmware was restored after the stress test. A separate 60-second capture
recorded 11 confirmed detections during spoken "yes" attempts. Reported LED state
alternated, and the user explicitly confirmed that the physical LED toggled.
The cumulative inference count reached 674; all readings reported zero audio
losses, event drops, stale events, notification errors, and inference errors.
Minimum unused stacks were 6784/4984 bytes (audio/control). Result transport
averaged 11,571 us with a 12,434 us maximum. Response measurements ranged from
about 201 to 458 ms from the first high-confidence window's estimated end.
These are not acoustic word-onset latency or dataset accuracy measurements.

Raw evidence: [rtos-spoken-yes.json](rtos-spoken-yes.json).

After this capture, the gap-handling regression was strengthened to retain the
release EMA for an already-latched detector. This avoids manufacturing release
from missing scores. Host tests and the final stress/restoration check were
repeated after that correction; nominal speech filtering is unchanged.


## Final-code regression and normal restoration

The strengthened latch-preservation regression and full host suite passed.
The 1500 ms backpressure experiment was repeated on the final code with a
15-second capture: again four event drops and nine stale-result rejections,
with zero reported audio loss, notification errors, or inference errors.
Normal firmware was rebuilt and uploaded by the test's restoration path.

A subsequent 15-second normal capture passed, with no audio/event/notification/
inference errors, no stale results, and measured unused stacks of 6784/4984 bytes.
The cumulative inference count reached 308. Average/maximum inference time was
58,630/59,652 us; nominal CPU timing varies across the recorded runs. The final
build manifest matches the maintained source, and the generated normal sketch
has no stress-test define.

Final-code evidence: [stress repeat](rtos-stress-final.json) and
[normal restoration](rtos-final-normal.json).

## Interpreting the stress result

The four score drops and nine stale-result rejections are expected outcomes of
this deliberate overload. At the nominal 128 ms decision cadence, the eight-slot
queue holds roughly one second of results, less than the 1500 ms injected stall.
Its producer never waits for room. Queue overflow advances the stream generation,
and control rejects old generations or scores older than 250 ms. The nine rejected
results include the score held by the stalled consumer and the older queued
scores. Increasing queue depth alone would not make delayed results fresh.

These counters refer to classification scores, not raw microphone samples.
Zero reported audio loss and continuing inferences show that the control stall
did not block the acquisition/processing path in this experiment. It does not
prove that every spoken command would be detected during a stall. Drops or stale
results in normal expected workloads would warrant investigation; neither was
observed in the normal captures.

## Measured memory margins

This table describes the original RTOS build's captures. Current-source stack
watermarks are 6872/4680 bytes and static RAM is 188,392 bytes, as summarized
above and in [the project README](../README.md#measured-memory-margins).

The lowest unused stack values across the recorded checks are 6784/4984 bytes
(audio/control), including the spoken-word and final normal captures. RTX reports
the unused stack watermark; used bytes below are allocation minus that watermark.

| Allocation | Capacity | Observed peak use | Remaining at that peak |
|---|---:|---:|---:|
| Audio-task stack | 8192 bytes | 1408 bytes | 6784 bytes (82.8%) |
| Control-task stack | 6144 bytes | 1160 bytes | 4984 bytes (81.1%) |
| Audio ring | 8192 samples | 768 samples | 7424 samples (90.6%) |

The ring has 512 ms nominal capacity at 16 kHz; its observed peak backlog is
48 ms, leaving 464 ms of capacity at that point. That margin absorbs bounded
processing delays, not sustained overload. The control-stall result measures a
different condition: the audio task continues running while control is blocked.

The 209,320-byte static RAM total already includes the application stacks and
96 KiB tensor arena. Do not subtract them again from the 52,824-byte remainder.
Successful model initialization verifies that tensor allocation fits its arena;
it does not quantify unused arena capacity. Runtime free/peak heap and
framework/interrupt-stack margins were not instrumented. These checks therefore
support the current application-stack/audio-buffer margins, not a complete
runtime memory budget or a guarantee for additional functionality.

## Scope of the evidence

All timings are observed measurements on the connected Nano, not established
worst-case execution times. JSON counters are cumulative since boot and may
include operation before the host capture begins; startup records can also be
buffered. Do not infer capture duration or a command success rate from the final
cumulative inference/trigger counts alone.

The spoken-word check establishes end-to-end microphone-to-LED operation, with
11 confirmed actions and physical observation. It does not establish false-accept
or false-reject rates: no labeled attempt count, controlled speaker/noise corpus,
or acoustic onset reference was recorded. The later latch-preservation correction
was host-regression/stress/normal tested as described above; a further physical
speech capture after that correction is not recorded here. Long-duration
endurance and automatic peripheral-failure recovery also remain future work.

## Developer-reported informal recognition

In the project discussion on October 20, 2026, the developer estimated roughly
**95% keyword-recognition success during informal use** and expressed satisfaction
with the current model. The inspected deployed model has **1075 parameter values**
(992 int8 weights and 83 int32 biases). A labeled attempt count and false-trigger
rate were not supplied with this estimate, so no statistical confidence or
generalized speaker/noise accuracy is established by it. The raw speech capture
records confirmed actions but has no labeled attempt denominator from which to
derive this percentage.

The suggestion that increased model capacity could raise recognition into the
**upper 90% range** is an untested hypothesis, not an additional hardware result.
Its rationale, limits, and proposed comparison are documented in
[TinyML inference](tinyml-inference.md#informal-recognition-and-possible-model-expansion).

## Object-based C++ refactor

Subsequent class/ownership changes, that stage's thirteen-entry host suite,
footprint, and repeated hardware checks are recorded separately in
[C++ refactor validation](cpp-refactor.md#verification). The earlier measurements
and captures above remain historical evidence for their stated builds.
The later [C++ idioms review](cpp-idioms.md#verification) adds the fourteenth suite
entry and records that stage's build and repeated speech/stress checks.

The [real-time review](realtime-review.md#verification) records the latest compiler
settings, reference interfaces, and repeated normal/stress/physical LED checks.
