# Zephyr real-time behavior and limits

The target is continuous keyword spotting with an LED output. The implementation
and tests support that workload; they are not a WCET proof or a safety
certification. [Validation](validation.md) identifies recorded hardware results.

## Bounded application work

| Boundary | Behavior |
| --- | --- |
| PDM callback | At most one configured 512-byte block per call, including partial/no-progress read handling; no model or Serial work |
| Audio ring | Fixed 8192 samples; reject new samples when full and invalidate subsequent detection evidence |
| Audio activation | At most eight hops, then a 1 ms sleep and explicit re-wakeup if the batch was full |
| Metrics | `K_NO_WAIT` mutex attempts; contention skips diagnostics instead of blocking audio |
| Scores | Eight value entries, nonblocking producer; overflow invalidates generation and can lose commands |
| Final action | Recursive scheduler lock around final generation/age validation and preinitialized GPIO only; interrupts stay enabled |
| Telemetry | Fixed formatting buffer and at most 64 bytes requested per pump; runs only in main at priority 6 |
| Inference failures | Counter/generation update; shared build strips blocking TFLite error strings |

The audio task intentionally waits when idle. Ordinary processing uses fixed
application buffers/stacks and no application heap allocation or deallocation.
An activation budget bounds the number of DSP/inference steps, not their WCET.
That does not imply the loader, USB or PDM driver never allocate or block.
Initialization messages and tensor-arena reporting occur before capture starts.

## Allocation and C++ policy

- Application storage is static or automatic with fixed extents: `std::array`,
  copied events, fixed Zephyr stacks/objects and a 64 KiB tensor arena. No growing
  containers, owning strings, type-erased callbacks or runtime resource creation
  occur in the application workers.
- The interpreter is placement-constructed in aligned member storage before
  capture. `AllocateTensors` plans the fixed arena before workers start. The
  pinned interpreter's `Invoke` uses that initialized arena; temporary kernel
  scratch storage remains inside it. Placement construction is not a heap call.
- The core PDM thread stack is allocated once at startup by the official driver.
  Its runtime audio buffers come from a fixed-size Zephyr slab. Slab returns are
  pool operations, not general heap allocation. USB/loader memory behavior remains
  outside the sketch's direct-reference audit.
- Target C++ compilation disables exceptions/RTTI and rejects variable-length
  arrays. The build inspects every sketch object's undefined symbols and rejects
  direct heap allocation/deallocation, exception and out-of-line atomic helpers.
  See `realtime-audit.json` beside the firmware for the actual imports checked.
  This check also rejects heap calls introduced in rarely used sketch paths.
- C++20 is used where it strengthens contracts: fixed extents, compile-time
  constraints, designated values, `constinit`, noncopyable owners and scoped
  synchronization. Lock-free atomic requirements are checked for the exact
  counter and pointer types. The single writer updates counters with loads/stores
  rather than retrying read/modify/write loops. No recursive application call path
  or runtime-sized application stack object is required.

Host tooling and test fixtures run on the development computer and may allocate
or use exceptions. They are not linked into the Nano firmware. The object audit
does not prove the absence of indirect allocation in libc, core or libraries,
nor does disabling exceptions prove bounded execution.

## Diagnostic isolation

The pinned Arduino `ZephyrSerial` implementation takes `K_FOREVER` semaphore
waits in `availableForWrite` and `write`; USB connection handling can sleep.
Checking available space therefore cannot establish nonblocking output. All
formatting, Serial calls and optional stack scans execute in the existing main
thread, lowered to priority 6 before releasing audio (4) and control (5).
Neither critical worker calls telemetry. Workers/main try the metrics mutex
with `K_NO_WAIT`; main releases it before formatting or USB access. Delayed
diagnostics may retain an older snapshot or stop reporting while recognition
continues. The core's own interrupt masking and system threads still require
target timing validation.

## Freshness and numerical hardening

Both micros and millis ages reject old scores, including a full microsecond
counter rollover. Generation/sequence checks discard stale or incomplete evidence
without erasing latch/cooldown suppression. Model metadata must have valid types,
size/data pointers, positive finite scales and int8 zero points. Prediction rejects
nonfinite/out-of-range scores without replacing the last valid probability.

Feature quantization rounds/clamps in floating point before conversion, avoiding
an overflowing intermediate int32 conversion for tiny valid scales. Uninitialized
extraction, failed matrix multiplication and negative/nonfinite mel energy fail
before integer conversion; DSP failures reset readiness, increment `dsp_errors`
and invalidate event generations. Failed/malformed PDM reads also reset context,
even if no application ring sample was dropped. Diagnostic probabilities are
checked/clamped independently before conversion. Deferred
inference counters saturate at their required threshold rather than growing
indefinitely. Packed FFT handling separates DC/Nyquist and respects exact extents.
Host fixtures cover these paths, but do not measure target execution times.

## Platform limits

The official PDM driver has its own slab/queue and dynamically allocated startup
thread stack. A slab/driver loss below the application ring is not necessarily
visible as `dropped`. Counter progress and audio age help reveal stalls; there
is no automatic peripheral restart, watchdog or comprehensive fault supervisor.
Startup failures within core/driver code are not exhaustively injected here.
The project stages a [PDM compatibility patch](../build-support/pdm/README.md)
that sleeps 1 ms after a negative device read. This fixes the upstream immediate
retry loop that could monopolize priority 3 on persistent failure. A host test of
the actual original/patched worker verifies the backoff and queue/callback fault
paths. Physical device-error injection and device-call WCET remain unmeasured;
the patch does not guarantee microphone recovery.

The stock loader's 128 KiB sketch RAM budget includes the fixed application
allocations. Its compiler warning about limited space remains visible. Arena use
was measured for the recorded model; peak system heap, core/interrupt stacks and
application stack watermarks remain unmeasured. `stack_free=0/0` is unavailable
information. A custom loader is required for supported watermark sampling.

Observed maxima include exercised scheduler/USB/driver interference but are not
worst-case bounds. IRQ-mask duration, maximum PDM callback time, schedulability
under added workloads and hours-to-days endurance have not been established.
Cold power-cycle, brownout, peripheral/allocation faults and memory-corruption
campaigns remain separate work. Fatal framework/TFLite assertions can halt the
system; LED state is not an actuator safety mechanism.

Unsigned event counters wrap intentionally. Timing averages rebase their own
window at its 32-bit limit, so count rollover cannot corrupt the average or
overflow its 64-bit sum. The event count remains separate. Dual-clock freshness assumes a queued result
cannot survive a full millisecond-clock cycle. These are practical limits for
very long deployments, not normal observed failures.

## Interpreting timing

`dsp_us` covers one extractor consumption, including the inexpensive priming hop.
`infer_us` covers input copy, model call and score decoding. `process_us` covers
one audio step, excluding later queue dispatch/metrics/control work. `event_us`
starts at inference completion and includes any injected post-dequeue control
stall. `response_us` starts at the estimated end of the first high-confidence
audio window, not acoustic word onset. Cumulative snapshots can include work
before recording starts. Speech accuracy needs labeled trials and false-trigger
measurement, not a count of LED actions.
