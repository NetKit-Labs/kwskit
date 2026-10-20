# Two-task RTOS validation - October 20, 2026

The firmware now has explicit audio and control application tasks on the Nano's
existing CMSIS-RTOS2/RTX kernel. The PDM callback remains interrupt driven.
See [RTOS design](rtos-design.md) for storage, priorities, and synchronization.

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
