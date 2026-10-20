# Locked command-line build validation — October 20, 2026

This is a historical record from before the two-task RTOS implementation.
Use the [build guide](build.md) for current commands, [RTOS design](rtos-design.md)
for task ownership, and [current verification](realtime-review.md#verification) for the
latest build footprint and hardware checks.

The new build was tested using project-local dependencies rather than the IDE's
installed core/libraries. No algorithm, model, queue, filter, or device-control
behavior was changed. `ARM_MATH_DSP` now has value `1`, matching the CMSIS header
and removing its redundant macro-definition warning.

## Checks performed

- `make setup`: downloaded the SHA-256-pinned native macOS ARM64 CLI, board core,
  Arm compiler/tools, and library archives into `.build/`.
- A fresh build exposed missing CMSIS headers in the CLI-installed tree. Those
  files were present in the official core archive. Setup now verifies/restores
  all regular core files from that explicitly checksum-pinned archive.
- The three previously local-only TFLite compatibility headers are tracked and
  installed by setup. No IDE library/core path is required by the build.
- `make test`: all existing firmware tests plus five build-tool tests passed.
  Added tests cover verified downloads, bad checksum rejection, archive path
  traversal rejection, library/lock drift, ambient Arduino configuration
  isolation, and restoration of missing/modified core files.
- `make build JOBS=4`: succeeded for `arduino:mbed_nano:nano33ble` using the locked
  CLI/core/compiler/libraries. A subsequent incremental build also succeeded.
- `make flash` without a port: rejected before compilation/upload.
- `make flash PORT=/dev/cu.usbmodem11201 JOBS=4`: built and uploaded successfully
  to the connected Nano 33 BLE.
- A fresh 15-second live capture passed with zero reported dropped samples,
  missed buffers, or inference errors. Cumulative inference count reached 171;
  average/maximum inference time was 59,094/60,103 µs. Peak queued samples: 768.
  One buffered startup record precedes the live records.

The locked build reports 328,880 bytes flash (33%), 194,112 bytes static RAM
(74%), and 68,032 bytes remaining. Flash is 576 bytes above the prior IDE-core
build; static RAM is unchanged. No byte-identical-build claim is made.

The archived libraries still emit upstream warnings when default compiler
warnings are enabled, and TFLite falls back to source compilation when its
precompiled binary is unavailable. These are not build failures.

Raw hardware evidence: [cli-build-telemetry.json](cli-build-telemetry.json).
Earlier spoken-word and physical LED validation remains in
[module-refactor-validation.md](module-refactor-validation.md).

## Hosted CI verification

The [final hosted run](https://github.com/jameslavrenz/KWS_Arm_CortexM4F/actions/runs/39477538812)
for commit `cb3ffba` passed both jobs: host tests and a clean Linux Nano firmware
build, including artifact upload. The workflow uses pinned Node 24 actions.
The preceding hosted build also reported exactly the same 328,880-byte flash and
194,112-byte static RAM usage as the local build. Downloadable firmware and build
provenance are available through the run's `nano33ble-firmware` artifact.

Hosted checks verify fresh-checkout tests/compilation; microphone and physical
LED behavior are verified by the separately recorded hardware sessions.


The later [RTOS design](rtos-design.md) and [RTOS validation](rtos-validation.md)
supersede this record for current task ownership, memory, and runtime results.
