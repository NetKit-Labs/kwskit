# Two-task RTOS design

The existing Arduino mbed/RTX kernel now runs two explicit application tasks.
The existing Arduino main task only waits; framework/USB/kernel threads remain.
The build and flashing commands in [build.md](build.md) are unchanged.

## Why two application tasks

The current firmware uses both tasks for every normal run. `kws-audio` owns DSP
and inference; `kws-control` owns score filtering, LED state, and telemetry.
Microphone acquisition executes in the PDM interrupt callback, outside both
tasks. It publishes samples and wakes audio processing using an ISR-safe flag.
Combining a short hardware callback with longer task-context processing is the
intended design; capture does not need its own application task.

The current keyword/LED workload could also run with interrupt-driven capture
and a cooperative main loop, as earlier versions did. The two-task structure
was introduced deliberately for independent scheduling and clearer ownership
as device functionality expands. The maintained firmware requires its RTOS
path; no cooperative-mode build option is provided. Two application tasks are
enough for the present workload, in addition to the existing framework threads.
Another task would be justified by a new independent timing/blocking requirement,
such as substantial communications work, and would need renewed budget checks.

## Scheduling and memory

| Context | Priority / allocation | Responsibility |
|---|---|---|
| PDM callback | Hardware IRQ priority 7 | Bounded PDM reads, atomic ring publication, ISR-safe thread flag |
| `kws-audio` | AboveNormal; 8192-byte static stack | Drain audio, overlap/FFT/log-mel, inference, nonblocking score sends |
| `kws-control` | Normal; 6144-byte static stack | Consume scores, temporal filtering, hysteresis/cooldown, LED actions, telemetry |
| Arduino main | Framework default; sleeps 1 s | Startup result and idling |

Both tasks execute on the same Cortex-M4F core; the scheduler preempts control
when the higher-priority audio task becomes ready.

The eight-entry CMSIS message queue copies `KeywordEvent` structs by value.
All task/queue/mutex control blocks and storage are statically allocated using
RTX size definitions; no application allocation occurs in the ISR or steady-state
message path. The 96 KiB tensor arena and feature/DSP workspaces remain static.
Unused task stack watermarks are reported through `stack_free`; they measure the
observed high-water usage, not a proof against every future execution path.

The current locked build reserves 188,392 bytes of static RAM, including both
stacks, the 16 KiB audio ring, and the 96 KiB tensor arena. Its remaining 73,752 bytes are
outside static allocations, not a free-heap measurement. The recorded stack
watermarks and ring occupancy are summarized in
[current validation](realtime-review.md#verification). Heap peak,
framework/interrupt-stack usage, and actual arena consumption remain unmeasured.

## Ownership and synchronization

After initialization, only the audio task calls feature extraction and the model.
Only the control task mutates the detector, drives the LED, and operates USB
telemetry. The PDM callback is the ring's sole producer; audio is its sole consumer.
Acquisition counters are copied with interrupts briefly disabled; `InterruptLock`
restores the caller's previous interrupt mask on scope exit. A priority-
inheriting mutex protects the audio statistics snapshot; it is never held across
DSP, inference, USB, GPIO, or a queue wait, and is never used by the ISR.
`MutexLock` uses timeout zero and releases only an acquired mutex. Audio skips
publication if busy; control retains its prior statistics. Metrics contention
therefore cannot suspend the audio task. Stack-watermark scans run initially
and at two-second intervals, rather than at every control wake.

The callback sets a thread flag after samples are available. Repeated flags can
coalesce safely because the ring preserves samples and the task drains it before
waiting again. An additional 100 ms timeout permits recovery from a failed wake
notification; `notify_errors` records such failures. The control task waits on
the event queue with a 20 ms timeout so disconnected/idle telemetry stays serviced.

## Events and overload

Each successful inference emits its probability, capture-window end, decision
time, sequence number, and stream generation. Low scores are included because
temporal smoothing and release/rearming require negative evidence too.

Queue sends have timeout zero. If control is delayed until the queue fills, audio
counts `event_drops`, advances the generation, and continues processing samples.
Audio loss and inference failure also advance the generation. Control rejects
older generations and events more than 250 ms old. Missing/duplicate/out-of-order
sequences or score gaps over 400 ms discard temporal evidence. The detector
retains its latch and cooldown across these gaps, preventing a fault from erasing
retrigger suppression. Fresh negative evidence still rearms the filter normally.

Filtering retains the original EMA 0.5, smoothed threshold 0.75, raw threshold
0.8, two qualifying decisions, release threshold 0.4, and 1500 ms cooldown.
The control task locks task scheduling for the final generation/dual-clock
freshness check and the preinitialized LED write. `SchedulerLock` restores the
previous scheduler state, including nested locking, and interrupts remain enabled.
This closes the preemption window between validation and action. A failed lock
suppresses the action. Never extend that scope with blocking communications or
dynamic allocation.

A slow control task can lose score events; it does not stall the audio consumer.
A slow audio task can still overflow the finite 512 ms audio ring. Neither queue
capacity nor task separation establishes a universal no-loss guarantee.

## Initialization failures

Model/DSP initialization precedes task creation. Both workers initially wait on
a start flag. PDM starts only after queue, mutex, and both tasks exist; startup
then checks both worker-release flag results. No blocking success print runs
after capture starts. TFLite error strings are stripped through the shared build
configuration, keeping inference failure reporting in counters rather than
blocking Arduino Serial writes. On failure, capture stops before task IDs, queue, or
mutex are deleted. Setup reports the failure and halts rather than running a
partially initialized application.

The application is a noncopyable function-local static, constructed after board
globals. Repeated successful `begin()` calls return without creating duplicate
resources. Its destructor performs cleanup; `AudioCapture` also releases its
driver/callback ownership on destruction. Normal operation runs continuously
while powered and does not terminate after a host recording interval.

## RTOS API source map

The application uses CMSIS-RTOS2 on the board core's existing Mbed/RTX kernel.
The framework initializes and starts the kernel; the project creates its workers
through the coordinator in [kws_pipeline.cpp](../firmware/kws_pipeline.cpp).

| API | Application use |
|---|---|
| `osThreadNew` | Create the audio and control tasks in `Application::begin` |
| `osThreadFlagsSet` / `osThreadFlagsWait` | Release workers after startup and notify/wake audio processing |
| `osMessageQueueNew` / `osMessageQueuePut` / `osMessageQueueGet` | Create the eight-score queue, send without waiting, receive with a 20 ms timeout |
| `osMutexNew` | Create the priority-inheriting metrics mutex |
| `osMutexAcquire` / `osMutexRelease` | Scope-based metrics protection in [mutex_lock.h](../firmware/mutex_lock.h) |
| `osThreadGetStackSpace` | Sample unused audio/control stack watermarks initially and every two seconds |
| `osKernelLock` / `osKernelRestoreLock` | Serialize final action validation and the initialized GPIO write; preserve interrupt delivery |
| `osKernelGetTickFreq` | Convert milliseconds to scheduler ticks |
| `osDelay` | Idle the Arduino main task for 1 s; inject the control stall only in the stress build |
| `osThreadTerminate` / `osMessageQueueDelete` / `osMutexDelete` | Release initialized resources during cleanup |

The interrupt callback never waits on a queue or mutex. Its thread-flag call
is ISR-safe; task-context waits, DSP, inference, filtering, GPIO actions, and USB
telemetry stay outside the callback.

## Hardware backpressure test

With pyserial available in the invoking Python environment, run:

```sh
python3 scripts/rtos_stress.py --port /dev/cu.usbmodem11201 --seconds 20 --jobs 4
```

The script builds a test-only generated sketch with `KWS_RTOS_STRESS_TEST` enabled,
which stalls control for 1500 ms on its first score. It checks that event queue
overflow and stale-result rejection occur while audio samples/inferences keep
progressing with zero reported audio losses. The test has a `finally` block that
rebuilds and uploads normal firmware after capture or a test failure. Confirm
normal restoration completes successfully; external process termination or power
loss can interrupt any restoration procedure. The maintained source and normal
Make build do not enable the stress flag. Test binaries/manifests are kept apart
in `.build/stress-artifacts/`.

Speech/LED behavior is a separate physical check; see [RTOS validation](rtos-validation.md).

`process_us` measures one audio pipeline step (hop handling, DSP and any
inference). It excludes queue dispatch, the metrics copy, and control-task work.
`event_us` measures time from inference completion to control-side validation
(including the injected delay after dequeue in the stress build), while
`response_us` includes filtering and control action delay from the first
high-confidence input-window end. Counters and timers are snapshots, not proofs
of hard worst-case execution times.

## C++ object ownership

A private application object owns the RTOS resources and capture/feature/control/telemetry
objects. `AudioPipeline` borrows its capture, extractor, and single model by reference.
Thread entry adapters receive the application as their CMSIS context; the PDM
notification also carries that context. Queue/event ownership and task priorities
are unchanged. See [C++ refactor](cpp-refactor.md) for storage and validation.

The [C++ idioms review](cpp-idioms.md) documents fixed-size buffer references,
scoped interrupt/mutex guards, automatic cleanup, and idempotent startup in the
current implementation. Scheduling, queue capacities, and task priorities remain
those described above.

See [real-time review](realtime-review.md) for the explicit callback work budget,
platform allocation/startup behavior, and remaining timing/fault-validation limits.
