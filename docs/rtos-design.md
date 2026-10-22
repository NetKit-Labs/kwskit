# Zephyr scheduling and ownership

## Why two application threads

The audio thread owns DSP/inference and stream recovery; control owns detection,
LED state and telemetry. The core's PDM thread acquires audio outside these two
application threads. Separate scheduling lets acquisition preempt a roughly
55 ms inference and lets audio run while control is delayed. All work executes
on one Cortex-M4F CPU; these are preemptible contexts, not parallel CPUs.
The Arduino main thread only initializes and sleeps for one second per poll.

## Scheduling and memory

| Context | Priority / allocation | Work |
| --- | --- | --- |
| Core PDM thread | Priority 3, configured with the library build flag | Read microphone blocks, invoke the application callback |
| Audio application thread | Priority 4, fixed 8192-byte member kernel stack | Drain ring, DSP, inference, nonblocking score sends |
| Control application thread | Priority 5, fixed 6144-byte member kernel stack | Score filtering, actions, telemetry |
| Arduino main and system threads | Core configuration | Startup, USB, kernel and other platform services |

Lower numbers mean higher priority. The PDM callback reads/publishes at most one
256-sample block per invocation and performs no feature extraction or inference.
An 8192-sample single-producer/single-consumer ring supplies 512 ms nominal
capacity. The eight-entry `k_msgq` copies `KeywordEvent` values; audio, feature
and tensor pointers never cross it. `K_KERNEL_STACK_MEMBER` provides aligned
member application stacks. Thread, mutex, semaphore and queue objects and
queue bytes are members of the static application object. The arena reserves
64 KiB. The core/driver can allocate during startup; fixed application storage
does not establish that the whole framework never allocates.

## Ownership and synchronization

Only the PDM callback advances the ring head; only audio advances the tail.
Release/acquire atomics publish samples without overwriting unread data. Short
`irq_lock`/`irq_unlock` snapshots preserve the caller's IRQ key on this single-core
board, covering producer-owned counters and arrival/backlog timestamps.
A binary semaphore with limit one coalesces notifications safely because sample
ownership lives in the ring. Audio drains complete hops before waiting again;
`k_sem_take` also has a 100 ms recovery timeout. `k_sem_give` has no failure return,
so the retained `notify_errors` field is zero for this backend.

A priority-inheriting `k_mutex` protects only the short metrics copy. Both threads
use `K_NO_WAIT`; audio can skip publication and control can retain its last
snapshot on contention. No mutex spans DSP, inference, queue waits, GPIO or USB.
Control receives scores with a 20 ms timeout, servicing telemetry when idle.
USB output checks connection and available space and writes at most 64 bytes per
pump. Firmware continues processing when the serial monitor is closed.

## Events and overload

Each successful inference emits probability, estimated capture-window end,
decision timestamps, sequence and stream generation. Low scores also travel to
control so release/rearming has negative evidence. Nonblocking score sends
preserve audio progress: queue overflow increments `event_drops` and the generation.
Sample loss or model failure also invalidates the generation. Audio loss discards
pending samples and resets feature readiness before rebuilding a full context;
a failed inference invalidates evidence without resetting DSP overlap.

Control rejects old generations and scores more than 250 ms old using both
microsecond and millisecond clocks. Missing/duplicate/out-of-order sequences or
score gaps over 400 ms discard partial temporal evidence while retaining latch
and cooldown. This prevents a gap from clearing retrigger suppression.

The filter uses EMA 0.5, raw threshold 0.8, smoothed threshold 0.75, two qualifying
decisions, release threshold 0.4 and 1500 ms cooldown. `k_sched_lock` and
`k_sched_unlock` enclose final generation/freshness validation and the already
initialized GPIO action. Zephyr locks are recursive and interrupts remain enabled.
Audio cannot invalidate the generation between that final check and action.
Never place waits, allocation, USB or a blocking device operation inside this scope.

A finite audio ring can still overflow if audio is delayed too long. Losing
classification results during overload can miss commands. Neither task separation
nor buffer capacity guarantees recognition under arbitrary load.

## Startup, cleanup and diagnostics

Model/tensor validation and DSP initialization precede kernel-object/thread
creation. Both application threads are created with start delay `K_FOREVER`.
The microphone starts only after shared objects exist; explicit `k_thread_start`
calls release control and audio afterward. Repeated successful `begin` calls
create no duplicate resources. On capture failure, cleanup stops the producer,
aborts created consumers, then purges the initialized message queue. Setup
reports failures and halts instead of running a partially initialized application.

The application is a noncopyable function-local static constructed after board
globals. Its destructor performs cleanup, and the capture owner also unregisters
its callback on destruction. Initialization messages and the arena-use print
precede capture; inference error strings are stripped from the blocking logger.

If a custom loader enables stack initialization and stack information, the code
samples `k_thread_stack_space_get` initially and every two seconds. The stock
loader lacks that capability/export: `stack_free=0/0` means unavailable. It is
not a measurement of stack exhaustion or free memory.

## Kernel API source map

| API | Use |
| --- | --- |
| `k_thread_create`, `k_thread_start`, `k_thread_abort` | Fixed-storage workers, gated startup and cleanup |
| `k_sem_init`, `k_sem_give`, `k_sem_take` | Coalesced capture notifications and audio wait |
| `k_msgq_init`, `k_msgq_put`, `k_msgq_get`, `k_msgq_purge` | Eight-value queue, nonblocking producer and bounded consumer wait |
| `k_mutex_init`, `k_mutex_lock`, `k_mutex_unlock` | Optional statistics protection |
| `k_sched_lock`, `k_sched_unlock` | Final freshness/generation check and GPIO action |
| `irq_lock`, `irq_unlock` | Nested capture snapshots |
| `k_msleep` | Idle Arduino main and test-only control delay |

Implementation: [coordinator](../firmware/kws_pipeline.cpp),
[capture](../firmware/audio_capture.cpp), [guards](../firmware/scheduler_lock.h).
[Testing](testing.md) covers unit, integration and physical backpressure procedures.
