# Zephyr scheduling and ownership

## Why Zephyr was selected

The current keyword-spotting behavior could be implemented without an RTOS,
using an interrupt-driven or event-driven application with careful buffering.
Zephyr is therefore not a fundamental requirement of the keyword-spotting
algorithm or the nRF52840 hardware.

For this project, however, the supported **Arduino Zephyr Boards** core provides
the vendor-supported PDM microphone, GPIO, timing and USB Serial APIs used by
the application. Selecting that core retains those existing libraries and
its Zephyr runtime, rather than replacing the board-support software or
introducing a separate scheduler. The application deliberately uses the native
kernel facilities already available in that environment.

Zephyr also provides a practical foundation for expanding the application.
Separating acquisition, DSP/inference, device control and best-effort telemetry
establishes ownership and scheduling boundaries that could accommodate
additional services, such as BLE communications for transmitting keyword
detection metrics or status, other sensors, or more complex control behavior.
**Those extensions are possibilities, not implemented features or validated
capacity.** Each new workload would require renewed CPU, stack, RAM, priority,
latency and end-to-end timing analysis to ensure that audio capture and keyword
detection remain reliable.

## Why two application threads

The audio thread owns DSP/inference and stream recovery; control owns detection
and LED state. Main owns best-effort telemetry and optional stack diagnostics.
The core's PDM thread acquires audio outside these two application threads.
Separate scheduling lets acquisition preempt a roughly
55 ms inference and lets audio run while control is delayed. All work executes
on one Cortex-M4F CPU; these are preemptible contexts, not parallel CPUs.
After initialization, Arduino main runs at priority 6 and sleeps 20 ms between
diagnostic pumps. It adds no application thread stack. Startup failure instead
sleeps for one second per poll, avoiding a busy-spin.

## Scheduling and memory

The Arduino core starts Zephyr before calling the sketch. The sketch creates
its application workers through the native kernel API; it does not start a
second scheduler or run a separate kernel. PDM, audio, control and main are
preemptible threads. The microphone's low-level hardware interrupts are handled
by the core/driver; the application callback runs in the core PDM thread.

| Context | Priority / allocation | Work |
| --- | --- | --- |
| Core PDM thread | Priority 3, configured with the library build flag | Read microphone blocks, invoke the application callback |
| Audio application thread | Priority 4, fixed 8192-byte member kernel stack | Drain ring, DSP, inference, nonblocking score sends |
| Control application thread | Priority 5, fixed 6144-byte member kernel stack | Score filtering and actions |
| Arduino main | Core-provided stack, priority set to 6 before workers start | Startup, best-effort telemetry and optional stack scans |
| System threads | Core configuration | USB, kernel and other platform services |

Lower numbers mean higher priority. The PDM callback reads/publishes at most one
256-sample block per invocation and performs no feature extraction or inference.
An 8192-sample single-producer/single-consumer ring supplies 512 ms nominal
capacity. The eight-entry `k_msgq` copies `KeywordEvent` values; audio, feature
and tensor pointers never cross it. `K_KERNEL_STACK_MEMBER` provides aligned
member application stacks. Thread, mutex, semaphore and queue objects and
queue bytes are members of the static application object. The arena reserves
64 KiB. The core/driver can allocate during startup; fixed application storage
does not establish that the whole framework never allocates.

The staged PDM worker adds a 1 ms sleep on negative device reads, so a persistent
read failure cannot turn its priority-3 loop into an immediate busy retry. This
[driver compatibility patch](../build-support/pdm/README.md) preserves normal
reads and does not allocate additional runtime storage.

## Ownership and synchronization

Only the PDM callback advances the ring head; only audio advances the tail.
Release/acquire atomics publish samples without overwriting unread data. Capture
counters and the active callback pointer use lock-free atomics; `volatile` is
not used for inter-thread synchronization. Single-writer counters use relaxed
loads/stores rather than retrying atomic read/modify/write loops. Short
`irq_lock`/`irq_unlock` snapshots preserve the caller's IRQ key on this single-core
board, covering producer-owned counters and arrival/backlog timestamps.
A binary semaphore with limit one coalesces notifications safely because sample
ownership lives in the ring. Audio drains at most eight complete hops per
activation; a full batch re-arms the semaphore and sleeps 1 ms so lower-priority
control can run even under sustained backlog. Otherwise it returns to the wait;
`k_sem_take` also has a 100 ms recovery timeout. `k_sem_give` has no failure return,
so the retained `notify_errors` field is zero for this backend.

A priority-inheriting `k_mutex` protects only the short metrics copy. Audio,
control and main use `K_NO_WAIT`; workers can skip publication and main can retain its last
snapshot on contention. No mutex spans DSP, inference, queue waits, GPIO or USB.
Control receives scores with a 20 ms timeout and publishes optional action metrics.
Main copies audio/control metrics, releases the mutex, then checks connection/space
and requests at most 64 Serial bytes. Core Serial methods can wait internally;
keeping them at priority 6 isolates them from both critical workers. Firmware
continues processing when the serial monitor is closed.

| Mechanism | Producer / consumer | Wait and contention policy |
| --- | --- | --- |
| Atomic sample ring | PDM callback / audio | No wait; reject new samples when full, then recover the stream |
| Binary `k_sem` | PDM callback / audio | Give without waiting; take for at most 100 ms, then inspect the ring again |
| Eight-entry `k_msgq` | Audio / control | Send with `K_NO_WAIT`; receive for at most 20 ms; overflow invalidates queued evidence |
| Metrics `k_mutex` | Audio/control publication / main snapshot | All use `K_NO_WAIT`; skip the copy if busy |
| `irq_lock` / `irq_unlock` | Capture snapshots | Short scalar snapshots; restore the caller's IRQ key, including nested scopes |
| `k_sched_lock` / `k_sched_unlock` | Control's final action | Short validation/GPIO scope; interrupts stay enabled and no blocking operation is allowed |

The semaphore is a wakeup hint, not a count of audio blocks. Multiple callbacks
may leave only one token, so audio inspects the ring rather than treating one
token as one block. Full batches explicitly request another activation.
The timeout also lets it inspect capture state when no callback arrives. The
bounded receive wait lets control publish diagnostics without a tight polling loop.

## Events and overload

Each successful inference emits probability, estimated capture-window end,
decision timestamps, sequence and stream generation. Low scores also travel to
control so release/rearming has negative evidence. Nonblocking score sends
preserve audio progress: queue overflow increments `event_drops` and the generation.
Sample loss, malformed/failed PDM reads, DSP failure or model failure also
invalidates the generation. Audio loss discards pending samples and resets
feature readiness before rebuilding a full context;
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

```mermaid
sequenceDiagram
    participant Main as Arduino main
    participant PDM as Core PDM (priority 3)
    participant Audio as Audio (priority 4)
    participant Control as Control (priority 5)
    Main->>Main: Initialize GPIO, serial, model and DSP
    Main->>Main: Initialize kernel objects; create dormant workers
    Main->>PDM: Start mono 16 kHz capture
    Main->>Main: Set main priority to 6
    Main->>Control: k_thread_start
    Main->>Audio: k_thread_start
    Note over Main: Best-effort diagnostics; poll sleeps 20 ms
    loop While capture runs
        PDM->>PDM: Publish bounded block into sample ring
        PDM->>Audio: k_sem_give (coalesced)
        Audio->>Audio: Drain complete hops; run DSP/inference when ready
        Audio->>Control: k_msgq_put score (K_NO_WAIT)
        Control->>Control: Check evidence, freshness and generation
        Control->>Control: Apply confirmed LED action
        Control-->>Main: Publish optional metrics (K_NO_WAIT)
        Main->>Main: Snapshot metrics; release lock; pump telemetry
    end
```

Arrows show ownership and API calls, not a promise of execution order after a
thread becomes ready. The higher-priority PDM/audio contexts can preempt control;
control can run while audio waits for more samples. The first inference needs
61 feature rows, and later inference is eligible after eight new rows; neither
the callback nor Arduino's `loop` performs this work.

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
| `k_current_get`, `k_thread_priority_set` | Lower main to priority 6 before releasing the workers |
| `k_sem_init`, `k_sem_give`, `k_sem_take` | Coalesced capture notifications and audio wait |
| `k_msgq_init`, `k_msgq_put`, `k_msgq_get`, `k_msgq_purge` | Eight-value queue, nonblocking producer and bounded consumer wait |
| `k_mutex_init`, `k_mutex_lock`, `k_mutex_unlock` | Optional statistics protection |
| `k_sched_lock`, `k_sched_unlock` | Final freshness/generation check and GPIO action |
| `irq_lock`, `irq_unlock` | Nested capture snapshots |
| `k_msleep` | Diagnostic main pacing, full audio-batch yield, startup-failure idle and test-only control delay |

Implementation: [coordinator](../firmware/kws_pipeline.cpp),
[capture](../firmware/audio_capture.cpp), [guards](../firmware/scheduler_lock.h).
[Testing](testing.md) covers unit, integration and physical backpressure procedures.
