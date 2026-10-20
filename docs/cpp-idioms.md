# C++ idioms and ownership review

This review covers every maintained native firmware source/header and Arduino
adapter, the host model inspector, native tests, and their fake driver/RTOS
interfaces. Generated model/filter data and the three upstream-derived TFLite
compatibility headers were checked as data/API boundaries. Downloaded board,
CMSIS, and TFLite implementations retain their original languages and APIs.
Python tooling/training and shell/build files retain their own languages.

## Changes with concrete guarantees

| Area | C++ design and guarantee |
|---|---|
| Capture / DSP / model handoff | `AudioHop` is `std::array<std::int16_t, 256>` and `FeatureTensor` is `std::array<std::int8_t, 1220>`. References preserve extents at the interface, so wrong-sized buffers cannot bind. `FeatureExtractor::features()` returns a const reference to its tensor. |
| FFT power | Array-reference template binds the real FFT length to its output extent (`N/2 + 1`), with an even-length compile-time assertion. DC/Nyquist handling stays explicit. |
| Numeric buffer movement | `std::copy` expresses value copying for overlapping left shifts and feature-row insertion. Destinations of the left shifts precede the source ranges, as required. Numeric move semantics add no resource-transfer behavior here. |
| Audio ring | Fixed `std::array` storage, private loss counter with a const accessor, nonzero power-of-two capacity checks, and a typed array-read overload. The lower-level pointer/count overload supports variable-length reads. |
| Loss recovery | The single consumer discards the current published backlog by advancing its atomic tail to a head snapshot. Samples published later remain queued. This needs no temporary discard buffer or drain loop. |
| Interrupt critical sections | Noncopyable `InterruptLock` saves/restores PRIMASK on scope entry/exit, preserving an already-disabled caller. Snapshots and timestamp reads cannot accidentally enable that caller's interrupts. |
| Metrics critical sections | Noncopyable `MutexLock` acquires an initialized CMSIS mutex and releases it on scope exit, including an early return. Guards cover only the short metrics copies. |
| Peripheral ownership | `AudioCapture` stops capture and clears its callback ownership on destruction. Capture, pipeline, extractor, model, device output, telemetry, and application owners cannot be copied. Pure filter/statistics value objects remain copyable. |
| Application lifetime | Function-local static application is constructed after board globals. Its destructor stops acquisition before releasing tasks, queues, and the mutex. Repeated successful `begin()` does not allocate duplicate RTOS resources. |
| RTOS transport/storage | Task stacks and queue bytes use aligned `std::array`; control blocks are value-initialized. A compile-time assertion requires `KeywordEvent` to remain trivially copyable for CMSIS message transport. |
| Standard library use | Maintained native code uses C++ standard headers, qualified fixed-width integer/size types, and explicit casts for representation conversions. `AudioPipeline` forward-declares borrowed module types. |

The core responsibilities remain in their respective types: `AudioCapture`,
`AudioPipeline`, `FeatureExtractor`, `KeywordModel`, `DeviceControl`, `Telemetry`,
and the private `Application`. Pure helpers such as `fftPower` appropriately
remain free functions. Simple event/result/statistics aggregates are ordinary
C++ value types; they do not require class hierarchies or custom constructors.

## Intentional low-level interfaces

PDM and CMSIS callbacks accept function pointers and context pointers. Small
adapters route them to stable, noncopyable owners. CMSIS DSP calls receive
`.data()` pointers only at their buffer boundary. Its matrix API takes a mutable
coefficient pointer even though multiplication reads the flash-resident filters;
the documented `const_cast` is confined to that API requirement.

USB sends byte representations, so its byte-pointer cast is deliberate.
`std::snprintf` is a standard C++ facility used for bounded telemetry output;
stream allocation and a new formatting dependency are unnecessary. TFLite's
interpreter is placement-constructed in aligned, privately owned static storage
and destroyed by that runtime's destructor. This supports fallible startup and
avoids allocating an interpreter from the application heap. The host inspector
already uses RAII file streams, vectors, sets, and exceptions; its metadata output
is checked against the saved model inspection.

These adapters provide ownership/lifetime guarantees while continuing to use
the existing platform/DSP implementations. No virtual dispatch or new dynamic
allocation was introduced in capture, DSP, inference, transport, or control.
The pinned board toolchain uses GNU++14; portable host tests use C++11.

## Verification

`make test` now has fourteen suite entries: ten component checks and four
integration/build checks. The new `buffer_contract` entry compiles ten probes
from `tests/fixtures/contracts`: valid calls must compile; wrong audio/feature/FFT
sizes, odd FFT lengths, invalid ring capacities, and copies of capture/extractor
owners must fail compilation. These intentionally rejected probes are kept
separate from executable firmware and are formatted with the other native code.

Behavioral tests also cover queue discard/reuse, owner destruction/late callbacks,
interrupt-mask preservation, scope-based mutex release, repeated startup, and the
existing partial-startup cleanup cases. The scalar-host extractor still matches
the 84,180-byte periodic-Hann feature reference. The host model inspector produces
metadata identical to `docs/model-metadata.json` under address/undefined-behavior
sanitizers. Full TensorFlow/CMSIS numerical parity and dataset accuracy remain
separate measurements; see [DSP verification](dsp-pipeline.md#training-alignment-and-verification-limits).

On October 20, 2026, the formatter gate, all fourteen host entries, and the final
Nano build passed. Flash is 332,120 bytes and static RAM is 209,328 bytes, an
increase of 184 and 8 bytes respectively from the preceding build. The 52,816-byte
remainder is not measured free heap. The model bytes, mel coefficients, and
periodic-Hann feature digest remain unchanged.

The 20-second [control-stall capture](cpp-idioms-stress.json) passed: four score
drops and nine stale-result rejections, zero reported audio sample/buffer losses,
and zero inference/notification errors. Audio and inference counters advanced;
peak ring occupancy was 768/8192 samples, with 6776/8192 and 4976/6144 bytes
reported unused in the audio/control stacks. The stress script restored the
updated normal firmware successfully. The developer then repeated spoken "yes"
attempts and confirmed that the built-in LED toggles.

The normal BIN is 332,716 bytes including image layout, SHA-256
`6705e8f2be8d5854974b9614d8186f8ed7645a20436b68a8938f5e37cb0b367c`.
The source manifest matches all 27 maintained firmware files.

A subsequent 60-second [normal-operation capture](cpp-idioms-normal.json) passed
with zero reported sample drops, missed buffers, score drops, stale results,
inference errors, or notification errors. The final cumulative statistics were
2.612/2.841 ms DSP average/maximum, 59.097/60.130 ms inference average/maximum,
and 62.823 ms maximum processing iteration. The counters reached 2,257,408
samples, 1,095 inferences, and 21 confirmed actions; these are cumulative since
boot, not counts for a controlled accuracy experiment. The first record was
buffered startup telemetry; the subsequent records span roughly one minute of
normal operation. Peak audio-ring occupancy was 768 samples and the minimum
reported unused task stacks were 6776/4976 bytes. The normal firmware remains
installed on the Nano, and the developer confirmed the physical LED response.

The [GitHub Actions run for firmware commit `2f47745`](https://github.com/jameslavrenz/KWS_Arm_CortexM4F/actions/runs/39504902995)
passed source formatting, host regressions, and the clean Linux Nano build.
A subsequent documentation refresh reran all fourteen host entries successfully
and checked the build manifest against all 27 maintained firmware files. That
refresh changed documentation only; it did not add a new hardware capture.
