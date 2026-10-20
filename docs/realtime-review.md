# Real-time execution review — October 20, 2026

This review covers maintained firmware and the relevant paths through the pinned
PDM, CMSIS-RTOS2/RTX, USB, and TFLite Micro dependencies. Its target is continuous
keyword spotting with a built-in LED. It is a code review plus targeted tests and
hardware observations, not a hard real-time or functional-safety certification.

## Findings and changes

| Finding | Change and verification |
|---|---|
| TFLite error reporting can reach blocking Arduino Serial writes during inference failure | The shared compiler configuration defines `TF_LITE_STRIP_ERROR_STRINGS`; application error counters and nonblocking telemetry remain. Upstream source bytes are unchanged. |
| A blocking startup-success print ran after capture started, while workers still waited for release | Removed that print; validate both worker start-flag results and stop capture/clean up if either fails. Startup regression injects both failures. |
| PDM callback draining had no explicit application work budget | Limit reads/publication to one configured 512-byte block per callback. Tests cover over-reporting, partial reads, no-progress reads, ordering, and overflow. The pinned driver normally provides one complete block. |
| Optional metrics used an indefinite mutex wait and released even if acquisition failed | Both tasks try the metrics mutex with timeout zero. Audio skips that diagnostic copy on contention; control retains its previous snapshot. The guard releases only an acquired mutex. Tests inject contention. |
| Audio could preempt between control's generation check and GPIO action | A scoped scheduler lock covers final generation/freshness validation and the preinitialized LED write. Interrupts remain enabled; prior scheduler state is restored. Tests simulate invalidation and delay immediately before locking, plus lock failure and nesting. |
| A stale event could look recent after a full microsecond-counter rollover | Validate age against both microsecond and millisecond timestamps. Reject nonfinite/out-of-range scores. Tests cover full microsecond rollover, normal clock/sequence wrap, and invalid probabilities. |
| Quantization converted to int32 before clamping | Clamp the rounded floating-point value before integer conversion; reject nonfinite/nonpositive input scales and invalid int8 zero points. The model adapter rejects invalid tensor metadata and preserves the last valid probability on prediction failure. Float-cast-overflow sanitizer tests cover tiny positive scales; the established feature digest remains the regression reference. |
| Deferred-inference frame count could grow indefinitely | Saturate at the required hop count. Deferred inference stays pending without signed-counter overflow; readiness/reset/cadence regressions remain. |
| Stack watermark scans ran on every control wake, inside RTX's SVC implementation | Scan initially and at two-second intervals, keeping the last values between samples. Actual coordinator tests check the scan cadence. Each scan remains bounded by the configured stack size. |
| Unused operator profiling reserved memory and added callbacks | Remove the 20,488-byte MicroProfiler object and pass no profiler to the interpreter. Whole-DSP/inference timing remains in application telemetry. |

The scheduling guard is limited to the existing LED action and small bookkeeping.
Do not put BLE, USB, delays, locks, allocation, or a future blocking device driver
inside that scope. LED configuration occurs before capture; the pinned Arduino
`digitalWrite` path reuses that initialized GPIO object.

The two application stacks, ring, score queue, mutex/thread control blocks,
feature workspace, and tensor arena remain fixed allocations. Normal application
processing has no heap allocation, recursion, variable-length arrays, blocking
Serial prints, or waiting score sends. The audio task may wait for audio/start
flags; those waits deliberately suspend an idle worker. Capture snapshots mask
interrupts only for short scalar copies and preserve the caller's prior mask.

## Platform limits and remaining evidence

A review cannot establish that every possible hardware/platform failure is safe:

- The pinned PDM driver's startup waits for the high-frequency oscillator without
  a timeout. Its double buffers use startup `realloc` followed by unchecked
  `memset`. Arduino GPIO setup also allocates. These upstream paths were inspected
  and left unchanged; startup is not proven bounded or safe under allocation
  failure. A product needing those guarantees needs a hardened driver/startup
  supervisor and allocation-failure testing.
- The PDM driver can disable capture on its own DMA-buffer overflow. The
  application does not provide automatic peripheral restart or a watchdog.
  Application ring-loss counters do not detect every possible hardware/driver
  loss. Telemetry age and sample progression help reveal stalled acquisition.
- Measured maxima are observations, not WCET bounds. ISR duration, maximum
  interrupt-masked time, all kernel/USB interference, and schedulability under
  added workloads have not been exhaustively measured. The scheduler guard and
  infrequent stack scans still add bounded scheduling delay.
- Application stack watermarks cover exercised paths. Peak runtime heap,
  framework/interrupt-stack use, and unused tensor-arena capacity remain
  unmeasured. Bytes outside static allocations are not measured free heap.
- There is no hours-to-days endurance record or comprehensive injected peripheral,
  allocation, kernel, brownout, or memory-corruption fault campaign. Fatal
  framework/TFLite assertions can halt the device. LED state is not a safety
  mechanism for an actuator.
- Unsigned telemetry counters wrap. `TimingStats` retains a 32-bit sample count;
  its cumulative average is not valid across count rollover (roughly 795 days
  at one record per 16 ms). This does not control detection, but uninterrupted
  multi-year telemetry needs explicit rebasing. Dual-clock event freshness also
  assumes queued events cannot survive an entire millisecond-clock cycle.

For deployment, define deadlines, allowed command loss, fault responses, and
uptime requirements first, then validate those requirements on the complete
hardware/software configuration. The present finite-buffer overload policy
preserves acquisition during a control stall by discarding classification scores;
it cannot guarantee recognition of every command during overload.

Relevant primary references: [CMSIS thread flags](https://arm-software.github.io/CMSIS_5/5.7.0/RTOS2/html/group__CMSIS__RTOS__ThreadFlagsMgmt.html),
[CMSIS scheduler locking](https://arm-software.github.io/CMSIS_5/5.7.0/RTOS2/html/group__CMSIS__RTOS__KernelCtrl.html),
and [RTX stack-watermark implementation](https://github.com/ARM-software/CMSIS_5/blob/5.7.0/CMSIS/RTOS2/RTX/Source/rtx_thread.c).
The installed core supplies its precompiled Mbed/RTX archive; the inspected
upstream RTX source explains the API implementation, not an independently
rebuilt or certified kernel.

## Verification

The fifteen-entry host gate and formatting check passed after these changes.
The extractor's 84,180-byte frozen feature digest is unchanged. Expanded tests
exercise actual capture/extractor/coordinator code with deterministic driver,
DSP, and RTOS fakes. Host scheduling simulations are not a concurrent hardware
race detector. Cross-build and physical verification follow below.

The added model-adapter entry has sixteen process-isolated scenarios using fake
TFLite tensors/statuses. It verifies initialization failures and bad metadata,
input copying, failed invocation, and nonfinite/out-of-range output rejection
without changing the last valid probability used by telemetry.

The normal Nano build and formatter checks passed. It uses **320,088 bytes flash**
and **188,392 bytes static RAM**, leaving **73,752 bytes outside static
allocations**. The previous build used 208,888 bytes static RAM; the reduction is
20,496 bytes, principally the unused profiler plus layout changes. This remainder
still is not measured free heap. ELF inspection found no linked `MicroPrintf` or
`DebugLog` function and no `MicroProfiler` implementation/object.

The final 20-second [control-stall regression](realtime-stress.json) passed:
four score drops, nine stale-result rejections, and zero reported sample/buffer
losses or inference/notification errors, with acquisition and inference advancing.
It reached a 60.705 ms processing-step maximum and 768 queued samples. The script
successfully rebuilt and uploaded normal firmware afterward.

The following 60-second [normal capture](realtime-normal.json) passed with 31
records, zero reported losses/errors, and zero score drops/stale results. The
final cumulative snapshot has 566 inferences and ten confirmed actions. The
developer separately confirmed that saying "yes" toggles the built-in LED with
this hardened normal image. Counts are cumulative since boot, can include time
before capture, and are not a labeled accuracy trial.

Normal DSP average/observed maximum was 1.987/2.129 ms, inference was
57.655/57.772 ms, and the longest observed processing step was 59.851 ms. Result
delivery averaged 8.595 ms with a 9.193 ms maximum. Reported unused audio/control
stacks reached 6872/4680 bytes across the normal/stress runs. These observations
support the present workload; they do not establish WCET, all-stack/heap margins,
or operation under arbitrary additional load.

The restored normal BIN is 320,664 bytes, SHA-256
`71714d71c948d9488895e6a93f4fd380ae1a438c7ce0f7c802e1764da5300df0`.
Its GNU++20 build manifest matches all 29 maintained firmware files and the
unchanged dependency pins. The trained model, mel coefficients, and imported
library/core source bytes are unchanged.
