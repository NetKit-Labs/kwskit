# Unit and regression testing

The project has host unit tests, integration/regression checks, and separate
hardware validation procedures. A regression test protects previously working
behavior; it may be a unit test or an integration test. The full regression gate
therefore includes all unit tests rather than treating the categories as exclusive.

## Run the automated host checks

From the repository root, with Python 3.9+, Make, and a host C++ compiler supporting
AddressSanitizer and UndefinedBehaviorSanitizer, with C++20 support:

```sh
make test-unit        # Ten component checks
make test-regression  # All fourteen checks, including the unit tests
make test             # Same complete regression gate used by CI
```

No Nano, Arduino IDE, dependency download, or `make setup` is needed for these
host tests. A failed compilation, assertion, sanitizer check, or Python test
returns a nonzero exit status and stops the suite. Most C++ checks run with
`-Wall -Wextra -Werror -fsanitize=address,undefined`; the fake-GPIO check uses
warnings as errors without sanitizers. Python harnesses compile actual firmware
modules against deterministic fakes where described below.
The device, pipeline, and RTOS fakes now live in separately formatted C++
source/header files under `tests/fixtures/`. The Python harnesses copy those
fixtures into temporary build directories and compile them with the actual
firmware modules; they no longer embed C++ programs in Python string literals.

## Unit coverage

| Check | Behavior protected |
|---|---|
| [audio_queue](../tests/audio_queue_test.cpp) | Empty/full behavior, rejected writes without overwrite, FIFO order, repeated ring-index wrap, referenced destinations, and incomplete/oversized reads leaving the queue and destination untouched |
| [fft_power](../tests/fft_power_test.cpp) | Separate DC/Nyquist power, interior complex-bin power, simultaneous first/last complex bins, signed inputs, and exact-size input/output bounds under sanitizers |
| [keyword_filter](../tests/keyword_filter_test.cpp) | Isolated-score suppression, sustained detection, hysteresis/rearming, held-keyword suppression, cooldown, and its exact expiration across millisecond rollover |
| [keyword_detector](../tests/keyword_detector_test.cpp) | Confirmation, reset behavior, response evidence, and microsecond timer rollover |
| [keyword_event](../tests/keyword_event_test.cpp) | Stale timestamps/generations, missing/duplicate sequences, cadence gaps, sequence/timer rollover, and preserving cooldown/latch evidence across gaps |
| [timing_stats](../tests/timing_stats_test.cpp) | Empty statistics, average/maximum/last values, and unsigned elapsed-time rollover |
| [device_action](../tests/device_action_test.py) | Actual device-control module initializes the LED off and alternates GPIO state on confirmed actions, using fake GPIO |
| [audio_capture](../tests/audio_capture_test.py) | Actual receiver with fake PDM: startup failure, exclusive callback ownership/context, bounded reads, overflow accounting without overwrite, complete-hop timestamps, discard, owner destruction, interrupt-mask preservation, late callbacks, and handoff to a new owner |
| [feature_extractor](../tests/feature_extractor_test.py) | Actual extractor with scalar host DSP backend: initialization failure, priming/warmup, eight-hop inference cadence, reset, independent contexts, clipping, and periodic Hann coefficients and 84,180 frozen quantized feature bytes |
| [buffer_contract](../tests/buffer_contract_test.py) | Eleven buffer/ownership compiler probes plus C++11/C++14/C++17 rejection checks: valid C++20 calls compile; wrong buffer extents, raw-pointer ring reads, invalid FFT lengths/capacities, and owner copies fail |

## Additional integration and regression coverage

| Check | Behavior protected |
|---|---|
| [stream](../tests/stream_test.cpp) | Ten simulated seconds of ordered 16 kHz audio while the consumer pauses for 60 ms inference intervals; no ring loss in the simulated workload |
| [pipeline](../tests/pipeline_test.py) | Actual audio pipeline with fake capture/features/model: no-hop/readiness behavior, backlog deferral, event timestamps, DSP/inference timing, inference failure, and loss-triggered feature reset |
| [build_tooling](../tests/build_tooling_test.py) | Nine Python cases for C++20 compiler options/provenance, compiler version/integrity/restoration, archive checksum/cache/path/link boundaries, library/lock changes, core restoration, and ambient Arduino isolation |
| [rtos_startup](../tests/rtos_startup_test.py) | Actual coordinator with fake CMSIS APIs: static stack/queue storage, task priorities, zero defaults for omitted designated fields, callback notification, idle delay, scoped mutex release, idempotent startup, and cleanup after seven initialization failure modes |

The fourteen checks are suite entries, not a claim of fourteen individual assertions;
each entry can exercise many cases. In particular, FFT/bin bounds, unread-audio
preservation, and latch retention guard against the project's earlier defects.

## Continuous integration and change workflow

The [GitHub Actions workflow](../.github/workflows/ci.yml) runs `make test` on
pushes and pull requests. Its source-style job also runs `make format-check` with
the [pinned formatters](source-style.md). Its separate firmware job runs `make setup` and
`make build JOBS=2` in a clean Linux environment and archives the firmware plus
source/dependency manifest. Host tests do not replace cross-compilation.

For a firmware change, add or update a behavioral test when appropriate, run the
full `make test` gate, and run `make build`. Changes to capture, scheduling,
inference, or actions also need the relevant hardware checks below. Preserve raw
telemetry and describe the build and workload before publishing new measurements.

## Hardware regression checks

The [build guide](build.md) covers upload and installation of pyserial. With a
connected Nano and normal firmware uploaded:

```sh
python3 tests/hardware_capture.py --port <Nano-serial-port> --seconds 60
```

The capture requires advancing audio/inference counters, zero reported
sample/buffer losses and inference/notification errors, no normal score drops or
stale results, ring occupancy below capacity, and reported unused task stacks
above 512 bytes. That stack threshold is a smoke-test floor, not a complete
memory-budget criterion. Speak "yes" with pauses of about three seconds and
separately confirm physical LED toggling; the script does not identify speech
attempts or calculate detection accuracy.

For deliberate control backpressure:

```sh
python3 scripts/rtos_stress.py --port <Nano-serial-port> --seconds 20 --jobs 4
```

This test builds/uploads a special image that stalls control for 1500 ms. Score
drops and stale-result rejection are required outcomes, while audio/inference
must continue without reported audio loss or inference/notification errors.
The recorded runs produced four score drops and nine stale rejections; the
automated checks require nonzero counts rather than an exact scheduler-dependent
count. The script rebuilds/uploads normal firmware in its restoration path;
verify that restoration completes. See [stress procedure](rtos-design.md#hardware-backpressure-test)
and [recorded hardware validation](rtos-validation.md).

## Recorded host verification

On October 20, 2026, `make test-unit` passed all seven component checks and
`make test-regression` passed all eleven suite entries, including the five
build-tooling Python cases. This run included the strengthened incomplete-read,
first/last FFT-bin, and millisecond-rollover cooldown assertions. The default
`make test` target resolves to the same complete suite used by CI. An invalid
suite selector was also verified to fail before executing tests.

This is host-test evidence. The physical hardware results remain the separately
recorded sessions in [RTOS validation](rtos-validation.md).

After standardizing four-space indentation and extracting the C++ fixtures,
`make format-check` and the full eleven-entry `make test` regression suite passed
again on October 20, 2026. Notebook code was checked for equivalent Python syntax
trees, and its saved training outputs were preserved. Build/artifact checks for
this change are recorded in [source-style validation](source-style.md#formatting-only-validation-before-the-object-refactor).

The earlier object refactor added two component checks, bringing its gate to
thirteen entries (nine component checks plus four integration/build checks).
See [C++ refactor verification](cpp-refactor.md#verification) for that build's
host, compile, and hardware evidence. The current frozen extractor vectors come from the procedural `a2c3c0d`
reference with periodic Hann applied, using the same deterministic PCM and scalar
host backend. Window sum and coefficient checks also run independently of that
feature digest. The historical implementation is not copied into the maintained
source tree. See [periodic-window validation](periodic-hann-validation.md).

The [C++ idioms review](cpp-idioms.md) adds typed buffer/FFT contracts, scoped
resource handling, and the compiler-probe entry. The current complete gate is
fourteen entries: ten component checks plus four integration/build checks.
Compiler-rejection fixtures are separate `.cpp` files and intentionally do not
compile when invoked as ordinary programs. That stage's normal/stress hardware
captures and physical LED confirmation are linked from that review; the earlier
[RTOS validation](rtos-validation.md) retains its original build measurements.

The earlier [C++17 update](cpp17-modernization.md#verification) selected that minimum
language mode in every host harness and both normal/stress cross-builds. It adds
a raw-pointer ring rejection probe, older-mode rejection checks, destination-size
regressions, and compiler/provenance tests. Its hardware captures and build
identity are retained there; the suite still has fourteen entries. The subsequent
[C++20 update](cpp20-modernization.md#verification) adds designated initialization,
the new pinned compiler, three rejected older modes, and nine build-tool cases.

## Limits

Host checks execute the actual feature extractor with a scalar DSP test backend
and the actual receiver with a PDM driver fake. They do not execute real PDM
interrupts, the CMSIS DSP implementation/scheduler, or the trained model. The stream simulation is deterministic,
not a concurrent interrupt/thread race detector. Sanitizers check the exercised
host code, not all memory accesses on the Nano. Dataset accuracy, acoustic-onset
latency, extended endurance, runtime heap/framework-stack margins, and unused
tensor-arena capacity remain outside the established test evidence.
The [DSP pipeline guide](dsp-pipeline.md#training-alignment-and-verification-limits)
details the shared-PCM feature comparisons still needed for full training/firmware
numerical parity. The window convention is periodic Hann in both implementations.
The [model inspection procedure](tinyml-inference.md#reproduce-the-artifact-inspection)
verifies the serialized graph, parameter counts, and tensor metadata using the
pinned schema headers after `make setup`. This optional host inspection is
separate from the dependency-free regression suite and does not execute inference
or establish quantized-model accuracy.
