# C++ object ownership and refactor validation

The procedural module APIs have been replaced by stateful C++ objects. This is
an ownership refactor: buffers, counters, and hardware state belong to the object
that manages them. Measurements below describe that build before the subsequent
[periodic-window update](periodic-hann-validation.md) and
[C++ idioms review](cpp-idioms.md). The latter records current buffer contracts,
resource cleanup, build footprint, and repeated hardware checks.
CMSIS-DSP, PDM, and TFLite keep their existing upstream APIs.

## Objects and lifetimes

| Component | State and ownership |
|---|---|
| `FeatureExtractor` | Private `std::array` buffers for framing, FFT, power, mel, and rolling quantized features; CMSIS descriptors and cadence counters. Const accessors expose readiness and model input. Copy/move is disabled because descriptors point into the object's buffers. |
| `AudioCapture` | PDM staging array, SPSC ring, counters, notification and context. A small static trampoline routes the driver callback to the active receiver. A second receiver cannot replace a running owner. `end()` stops acquisition before clearing that owner. |
| `AudioPipeline` | Borrows capture, extractor, and model by reference through its constructor. Owns its hop workspace, timing statistics, and loss-recovery state. |
| `KeywordModel` | A noncopyable single-model facade with one private `Runtime`. That runtime owns its 96 KiB arena, resolver, profiler, tensors, and interpreter storage. `instance()` supplies its single static lifetime and avoids exposing TFLite implementation types to consumers. |
| `DeviceControl` | Its LED state is a private member. Confirmed events invoke the existing toggle action. |
| `Telemetry` | Private fixed-size staging array, send offset/length, and reporting timestamp. Its nonblocking USB protocol is preserved. |
| Private `Application` | Owns capture, extractor, pipeline, device, telemetry, RTOS handles/control blocks, stacks, event storage, counters, and shared metrics. Thread adapters use the CMSIS context pointer; PDM notifications also carry this application context. |

The application and model runtime have static lifetime. The approximately 10 KiB
DSP workspace and 96 KiB model arena therefore remain in static SRAM, rather than
on the audio task stack. The model interpreter is placement-constructed in
aligned member storage during `begin()`; placement construction does not request
heap memory. Its lifetime ends with the runtime. Startup can report hardware or
allocation failure through the existing `begin()` result; RTOS partial-startup
cleanup still stops capture before deleting task resources.

The audio task remains the exclusive DSP/model user after initialization.
Control owns filtering/actions/telemetry. Buffers do not cross the event queue.
These ownership rules do not make concurrent calls on the same extractor/model
safe; synchronization and scheduling rules remain those in [RTOS design](rtos-design.md).

Namespaces remain appropriate for stateless helpers and implementation-local
types. `KwsPipeline::begin` and `poll` are thin entry points for the minimal Arduino
adapter; an unnamed namespace keeps the private application type internal to its
translation unit. A namespace is ordinary C++ scope, not a way to convert C into
C++ or bypass compiler checks.

## Preserved behavior and targeted correction

- 16 kHz acquisition, 8192-sample ring, 256-sample hops, packed 512-point real FFT,
  mel/log/quantization formulas, 61-row context, and eight-hop inference cadence.
- Static task stacks, priorities, eight-event transport, freshness/generation
  filtering, smoothing/hysteresis/cooldown, LED actions, and telemetry fields.
- Fixed-size buffers and no new application heap allocation in capture, DSP,
  inference, event transport, or control.
- Backlog-to-time conversion now widens before multiplying by 1,000,000. The old
  32-bit target expression could overflow for more than 4294 queued samples;
  the 8192-sample ring permits that depth. The full-ring timestamp test covers
  the intended calculation without dropping the high product bits.

## Verification

On October 20, 2026:

- `make test`: all thirteen entries passed (nine component checks plus four
  integration/build checks), including sanitizer runs.
- The actual extractor produced 84,180 quantized bytes identical to commit
  `a2c3c0d` on the same scalar host DSP backend. Deterministic PCM includes
  silence, a periodic tone, and noise at three quantization scales, including
  clipping. Initialization failure, priming/warmup, reset, independent object
  state, and inference cadence are also asserted.
- The actual capture module passed fake-driver tests for failed startup,
  exclusive callback/context ownership, bounded reads, overflow preservation,
  timestamp calculation, discard, late callbacks, and handoff to another owner.
- `make build JOBS=4`: 331,936 bytes flash and 209,320 bytes static RAM. Relative
  to the formatting-only build, flash increased by 496 bytes and static RAM
  stayed the same. The 52,824-byte remainder is not a measured free-heap value.

The 20-second [hardware stress capture](cpp-refactor-stress.json) passed after
uploading the special 1500 ms control-stall image: four score drops, nine stale
rejections, zero reported audio loss, and zero inference/notification errors.
Normal firmware was rebuilt and uploaded successfully afterward. The observed
stress stack-free watermarks were 6776/8192 bytes for audio and 4976/6144 for
control; the ring peaked at 768/8192 samples.
The scalar host backend does not execute CMSIS. Its frozen results check refactor
equivalence, not training/firmware parity or acoustic accuracy.
[DSP verification limits](dsp-pipeline.md#training-alignment-and-verification-limits)
remain documented. Observed stack watermarks and timings describe exercised
paths rather than established worst-case guarantees.

The 60-second [normal capture](cpp-refactor-normal.json) also passed. Audio and
inference counters advanced; audio losses, missed buffers, inference/notification
errors, score drops, and stale rejections remained zero. The final cumulative
DSP average/maximum was 2627/2853 us, inference 59102/59233 us, and processing-step
maximum 61997 us. Observed unused stack was 6776 bytes for audio and 4976 for
control; peak ring occupancy was 768 samples. Counters are cumulative since boot,
not restricted to the host recording interval. Normal firmware was restored after that run.

No confirmed keyword actions occurred in the automated normal capture. After
normal-firmware restoration, the developer separately repeated the spoken
"yes" test and confirmed that the built-in LED toggles. This is a fresh physical
end-to-end confirmation for the object-based firmware; no labeled attempt count
or accuracy rate is inferred from it. Earlier speech/LED captures remain in
[RTOS validation](rtos-validation.md#spoken-word-and-physical-led-check).

The normal BIN is 332,532 bytes including image layout, with SHA-256
`89178085786d3ee3de5a38da7b2cdbca22b0d83b6ce79691f680e37e3b70b711`.
That build manifest matched the firmware sources at the time of the object refactor.
