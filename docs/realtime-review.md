# Zephyr real-time behavior and limits

The target is continuous keyword spotting with an LED output. The implementation
and tests support that workload; they are not a WCET proof or a safety
certification. [Validation](validation.md) identifies recorded hardware results.

## Bounded application work

| Boundary | Behavior |
| --- | --- |
| PDM callback | At most one configured 512-byte block per call, including partial/no-progress read handling; no model or Serial work |
| Audio ring | Fixed 8192 samples; reject new samples when full and invalidate subsequent detection evidence |
| Metrics | `K_NO_WAIT` mutex attempts; contention skips diagnostics instead of blocking audio |
| Scores | Eight value entries, nonblocking producer; overflow invalidates generation and can lose commands |
| Final action | Recursive scheduler lock around final generation/age validation and preinitialized GPIO only; interrupts stay enabled |
| Telemetry | Fixed formatting buffer, connected/available-space checks and at most 64 bytes per pump |
| Inference failures | Counter/generation update; shared build strips blocking TFLite error strings |

The audio task intentionally waits when idle. Ordinary processing uses fixed
application buffers/stacks and no per-inference application heap allocation.
That does not imply the loader, USB or PDM driver never allocate or block.
Initialization messages and tensor-arena reporting occur before capture starts.

## Freshness and numerical hardening

Both micros and millis ages reject old scores, including a full microsecond
counter rollover. Generation/sequence checks discard stale or incomplete evidence
without erasing latch/cooldown suppression. Model metadata must have valid types,
size/data pointers, positive finite scales and int8 zero points. Prediction rejects
nonfinite/out-of-range scores without replacing the last valid probability.

Feature quantization rounds/clamps in floating point before conversion, avoiding
an overflowing intermediate int32 conversion for tiny valid scales. Deferred
inference counters saturate at their required threshold rather than growing
indefinitely. Packed FFT handling separates DC/Nyquist and respects exact extents.
Host fixtures cover these paths, but do not measure target execution times.

## Platform limits

The official PDM driver has its own slab/queue and dynamically allocated startup
thread stack. A slab/driver loss below the application ring is not necessarily
visible as `dropped`. Counter progress and audio age help reveal stalls; there
is no automatic peripheral restart, watchdog or comprehensive fault supervisor.
Startup failures within core/driver code are not exhaustively injected here.

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

Unsigned counters wrap. Cumulative timing averages need rebasing before their
32-bit sample count rolls over; dual-clock freshness assumes a queued result
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
