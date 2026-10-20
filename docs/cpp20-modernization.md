# C++20 initialization and compiler update

This record describes the C++20 migration build. The later [real-time review](realtime-review.md)
supersedes its current footprint, synchronization, and hardware measurements.

The maintained native firmware, tests, and host inspector now require C++20.
Firmware selects GNU++20 with Arm GNU Toolchain 13.3.Rel1 (GCC 13.3.1); host checks
use C++20. The project header rejects older modes, including C++17. The board
package's bundled GCC 7.2.1 does not support the required standard, so changing
only its language flag would not provide a C++20 build.

## Designated initialization

CMSIS mutex, message-queue, and thread attributes use named C++20 initializers.
The audio/control threads have separate const configuration objects; no mutable
attribute structure is reused between workers. Designators follow declaration
order as required by C++20. Omitted members are value-initialized to zero/null,
including unused thread flags, TrustZone module, and reserved fields. The fake
CMSIS startup checks verify those defaults along with priorities and storage.

Acquisition snapshots and keyword-score events also name their initialized
members. Empty `{}` initialization remains appropriate for initially empty
buffers/control blocks; objects with constructors retain their existing APIs.

## Other focused changes

- `std::has_single_bit` expresses the ring's compile-time nonzero power-of-two rule.
- `std::numbers::pi_v<double>` supplies the same periodic-Hann constant used by the
  existing feature calculation; the frozen feature regression checks its output.
- `constinit` ensures that the nullable PDM callback owner has static initialization.
- The host inspector uses `std::set::contains` for parameter membership checks.
- ISR-owned volatile counters use explicit reads/assignments instead of the
  compound increment operators deprecated for volatile objects in C++20. Snapshot
  synchronization and the single-interrupt-producer ownership rule are unchanged.

## Pinned compiler and platform boundary

[dependencies.lock.json](../dependencies.lock.json) pins the active compiler's
four macOS/Linux host archives using Arm's published SHA-256 values. Setup
extracts it under `.build/toolchain/`; build verifies its installed file manifest
and reported GCC version. Archive paths and link targets must stay inside the
extraction directory. The normal and control-stall builds share the same compiler
selection helper, and their manifests record the language mode and dependency pins.

The board/core/CMSIS/TFLite source contents remain unchanged. The explicit
`compiler.path` override selects the new compiler, assembler, linker, and binary
tools for the Arduino recipes. Imported C sources still use C recipes. The core's
precompiled Mbed/RTX archive remains the supplied archive; its source is not
rebuilt by this project. Cross-build and physical regression results below cover
this compiler/platform combination on the connected Nano.

Vendor provenance: [Arm GNU Toolchain release notes](https://documentation-service.arm.com/static/66bb3acb882fec713ef48f84)
and [Arm's compiler distribution](https://developer.arm.com/-/media/Files/downloads/gnu/13.3.rel1/binrel/).
The lockfile contains the exact filenames and checksums, including the native
Apple Silicon distribution. See [build configuration](build.md#c20-compiler-configuration)
for Make and optional Arduino IDE setup.

## Verification

The full host gate has fourteen entries. The compiler-contract entry checks
valid C++20 code, eleven buffer/ownership probes, and rejection of C++11/14/17.
The build-tool entry has nine Python cases, including compiler version/integrity,
archive path/link boundaries, unchanged-file reuse, and restoration after changes.

The formatter, complete host gate, and Nano cross-build passed. The scalar-host
extractor still matches the 84,180-byte frozen feature reference; the sanitizer
model inspector produces metadata identical to `docs/model-metadata.json`.
The normal build uses 331,488 bytes flash and 208,888 bytes static RAM, leaving
53,256 bytes outside static allocations. This is 128 bytes less flash and 440
bytes less static RAM than the previous C++17 build. The remainder is not
measured free heap; task watermarks do not measure framework/interrupt stacks.

The 20-second [control-stall test](cpp20-stress.json) passed with four score drops,
nine stale-result rejections, and zero reported sample/buffer losses or inference/
notification errors. Audio and inference counts continued. Peak ring occupancy
was 768 samples; reported unused audio/control stacks reached 6856/4848 bytes.
Normal firmware was rebuilt and uploaded successfully after the test.

The normal BIN is 332,064 bytes including image layout, SHA-256
`da89f4fdba05389c9b09f93e4d4f08af34c61a6c01ddfc9e749930a5256a76ae`.
Its GNU++20 manifest matches all 28 maintained firmware files.

The subsequent 60-second [normal capture](cpp20-normal.json) passed with 31
records and zero reported sample drops, missed buffers, inference/notification
errors, score drops, or stale results. Its final cumulative snapshot reports
921 inferences and 13 confirmed actions. The developer physically confirmed that
saying "yes" still toggles the built-in LED with this C++20 normal image.
Counters are cumulative since boot and can include time before capture; action
counts do not establish classification accuracy.

Normal DSP average/observed maximum was 1.985/3.015 ms; inference was
57.914/57.954 ms; the longest observed processing step was 60.030 ms. Result
delivery averaged 9.001 ms with a 9.630 ms observed maximum. Minimum unused
application stacks across both captures were 6856/4848 bytes (audio/control).
These are exercised-path observations rather than worst-case guarantees.
The full updated `make setup` workflow also completed successfully, including
verification and reuse of the installed pinned compiler and dependencies.
