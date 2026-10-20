# C++17 baseline and reference interfaces

The maintained native application, tests, and host model inspector require C++17
or later. Firmware builds select GNU++17, host checks use C++17, and the formatter
uses its C++17 grammar. No current feature requires C++20, and no C++20 firmware
configuration is claimed for the pinned GCC 7.2.1 toolchain.

## Build contract

`firmware/cpp_standard.h` rejects `__cplusplus` values below `201703L`.
The normal build and hardware stress/restoration share
`scripts/project.py::compile_sketch`, which passes the explicit language mode
and preincludes that header. The header loads `<chrono>` before Arduino defines
its `abs` macro, avoiding a collision with the C++17 chrono overload.

The downloaded board, CMSIS, and TFLite sources are unchanged. Their compiled
C++ translation units receive the same mode and include-order fix; imported C
translation units still use the C recipe, and the existing Mbed archive remains
precompiled. Both normal and stress manifests record `cpp_standard: gnu++17`.
See [the build guide](build.md#c17-compiler-configuration) for automatic Make
settings and the equivalent optional Arduino IDE configuration.

## Code changes

| Area | Change |
|---|---|
| Audio queue | `pop` accepts a `std::array` reference and optional requested count. The array carries its capacity; oversized requests fail before reading or advancing the queue. The public raw-pointer overload is removed. |
| Inference | Required model/tensor objects use local references once available. Nullable tensor/interpreter members remain pointers because initialization can fail; tensor pointers are checked before references bind. |
| Interpreter storage | Aligned `std::array<std::byte, sizeof(MicroInterpreter)>` holds the placement-constructed interpreter without application heap allocation. |
| Quantization | `std::clamp` expresses the unchanged int8 saturation bounds. |
| Queue transport | `std::is_trivially_copyable_v` checks that keyword values can cross the CMSIS byte-copy queue. |
| Host inspector | Required graph/operator/tensor objects and the parameter-count helper use const references. JSON strings use `std::string_view` and a range loop. Optional FlatBuffer vectors/quantization fields retain nullable pointers. |

Existing capture/extractor/model dependencies, audio hops, and feature tensors
already used references. PDM/CMSIS callbacks, USB byte buffers, reseatable RTOS
handles, inactive capture ownership, and uninitialized runtime state appropriately
retain pointers. Changing a required API signature or inventing a dummy object
for a null state would obscure those contracts.

## Verification

The complete host gate still has fourteen entries. Its buffer-contract entry
now compiles eleven buffer/ownership probes and rejects C++11/C++14 explicitly.
The audio-ring tests verify that an oversized requested count preserves unread
samples and destination contents. The build-tool entry now has seven Python
cases, including normal/stress compiler settings and language/source provenance.
The scalar-host feature digest and deployed-model metadata are checked for
regression; these do not establish complete TensorFlow/CMSIS numerical parity.

`make format-check`, all fourteen host entries (including seven build-tool cases),
and the Nano cross-build passed. The firmware uses 331,616 bytes of flash and
209,328 bytes of static RAM, leaving 52,816 bytes outside static allocations.
Flash is 504 bytes lower than the preceding GNU++14 image; static RAM is unchanged.
This remainder is not measured free heap, and the stack/ring checks are exercised
margins rather than worst-case guarantees.

The sanitizer-built C++17 model inspector produces metadata identical to
`docs/model-metadata.json`. The frozen scalar-host feature digest remains
`c14a17d8294d72bc45082f4bd525fd15a93f77390b5e05658a9e31f969327d81`
for 84,180 quantized bytes. Model bytes, mel data, dependency pins, FFT bin handling,
periodic Hann convention, and filter thresholds are unchanged.

The 20-second [control-stall capture](cpp17-stress.json) passed with four score
drops and nine stale-result rejections, zero reported sample/buffer losses, and
zero inference/notification errors. Audio and inference counters continued while
control was stalled for 1500 ms. Peak ring occupancy was 768/8192 samples; reported
unused stacks reached 6768/8192 and 4992/6144 bytes for audio/control in that test.
The script rebuilt and uploaded the normal image successfully.

The normal BIN is 332,212 bytes including image layout, SHA-256
`bedb469f64c5c409c5efd33c7514fd8ff438e8e22b48fd1d3672a5a4e0ca5801`.
Its GNU++17 manifest matches all 28 maintained firmware files, including the
language guard. Source staging preserves timestamps for unchanged content so
the forced include does not trigger a full dependency rebuild on every invocation.

The developer repeated spoken "yes" attempts with the restored normal firmware
and explicitly confirmed that the built-in LED toggles. This is a functional
microphone-to-output check, not a labeled accuracy or false-trigger measurement.

The subsequent 60-second [normal capture](cpp17-normal.json) passed with zero
reported sample drops, missed buffers, inference/notification errors, score drops,
or stale results. Final cumulative values were 1,202,688 received samples,
580 inferences, and 15 confirmed actions. DSP average/maximum was 2.487/2.639 ms,
inference 59.117/60.183 ms, and maximum pipeline step 62.766 ms. Result delivery
averaged 11.021 ms with an 11.669 ms observed maximum. Peak ring occupancy was
768/8192 samples; minimum reported unused stacks were 6768/8192 and 4976/6144
bytes (audio/control). These counters are cumulative since boot and can include
operation before host recording; they are not a trial denominator. Normal C++17
firmware remains installed and runs continuously while powered.
