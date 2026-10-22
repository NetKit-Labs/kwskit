# Arduino Zephyr port

This branch makes Arduino Zephyr Boards **1.0.0** the default for the Nano 33 BLE
Sense (nRF52840). The original Mbed backend remains selected automatically when
building for the Mbed core. No model, filter coefficients, keyword thresholds,
FFT layout, inference cadence, or GPIO action has changed.

## Build and upload on macOS

```sh
make setup
make test
make build JOBS=4
make boards
make flash PORT=/dev/cu.usbmodem11301 JOBS=4
python3 tests/hardware_capture.py --port /dev/cu.usbmodem11301 --seconds 60 \
    --allow-unavailable-stack-metrics --output .build/zephyr-normal.json
python3 scripts/rtos_stress.py --port /dev/cu.usbmodem11301 --seconds 25 \
    --output .build/zephyr-stress.json
```

Choose the actual USB port reported by `make boards`. Hardware capture needs
PySerial (`python3 -m pip install pyserial` in a virtual environment if absent).
Setup installs the locked official Arduino CLI 1.5.1, core, and libraries under
`.build/`; no global Arduino installation or sketchbook is modified. The core
is available in Arduino's standard package index. The older additional Zephyr
index contains beta releases and is not needed.

The FQBN is `arduino:zephyr_main:nano33ble:link_mode=static`. The CLI installs and
selects the official `zephyr:arm-zephyr-eabi@1.0.1` tool (GCC 14.3.0), without a
compiler-path override. C++20 is retained. Outputs and provenance are written to
`.build/zephyr-artifacts/`, with separate staging and object directories from
Mbed. The ELF, BIN and HEX files are intermediate outputs; use Arduino CLI's
upload recipe, which selects the packaged `bin-zsk.bin` for static mode.

Arduino's upload recipe installs/updates the matching Zephyr loader automatically
before the sketch. It preserves the board's original USB bootloader. If the
1200-baud reset cannot enter that bootloader, double-press RESET and rerun the
upload with its newly enumerated port. Close other serial monitors first.

## Changes and compatibility

| Existing dependency | Zephyr implementation |
| --- | --- |
| CMSIS-RTOS2/RTX threads | `k_thread_create`, fixed 8192/6144-byte embedded kernel stacks |
| Run flags | Both threads created with `K_FOREVER`; explicitly started after successful capture initialization |
| Audio callback notification | Binary `k_sem`, coalesced and safe to give from interrupt or thread context |
| Eight score events | Fixed storage `k_msgq`, nonblocking producer and 20 ms consumer timeout |
| Metrics mutex | `k_mutex`, nonblocking snapshots, priority inheritance provided by Zephyr |
| Scheduler critical section | Recursive `k_sched_lock`/`k_sched_unlock` around the final generation check and GPIO action |
| Interrupt mask preservation | `irq_lock`/`irq_unlock` |
| Mbed USB `send_nb` | Arduino `Serial.availableForWrite()` and bounded `Serial.write()` |
| RTX stack watermark scan | Zero means unavailable on the unmodified stock loader |

The bundled Arduino PDM library retains `PDM.begin(1, 16000)`, callback, read,
gain 60, and end APIs. Its callbacks run in a dedicated thread. The core lacks
`setBufferSize`, so compile flags configure `PDM_NUMBER_OF_SAMPLES=256` in both
the application and library. Its producer thread uses priority 3, ahead of audio
priority 4 and control priority 5 (lower numbers run first in Zephyr). Each
callback still reads at most one 256-sample block. The existing single producer /
single consumer ring and atomic publication remain intact. These critical
sections target the single-core nRF52840.

Arduino_CMSIS-DSP 5.7.0 is built from source for the Cortex-M4F hard-float ABI.
Its library metadata lists only Mbed architectures, so Arduino CLI warns about
the architecture. The pinned TensorFlow Lite revision and CMSIS-NN kernels are
retained. Setup guards only its unused Mbed/I2S peripheral example helpers and
serial-test constructor for Zephyr; the inference sources are unchanged. See
[the compatibility notes](../build-support/tflm-compat/README.md).

The stock loader reserves **128 KiB** for the sketch. Dynamic linking initially
required 205,578 bytes because it also places sketch code in RAM. The supported
static mode keeps code in flash. The Zephyr tensor arena reserves **64 KiB**;
Mbed still reserves 96 KiB. Model initialization checks allocation and prints
`tensor_arena_used=...` on Zephyr. The model and all quantization parameters are
unchanged. No custom loader, Zephyr SDK installation, or west build is required.

The stock loader has `CONFIG_INIT_STACKS` disabled and does not export
`k_thread_stack_space_get`. `stack_free=0/0` therefore reports unavailable data,
not exhausted stacks. The optional hardware-check flag accepts exactly that
pair; it continues to check reported sample losses, inference errors, event
losses, audio age, queue capacity, and advancing audio/inference counters.
Measured stack headroom requires a custom loader and remains a manual follow-up.

## Preserved Mbed build and rollback

```sh
KWS_CORE=mbed make setup
KWS_CORE=mbed make build
KWS_CORE=mbed make flash PORT=/dev/cu.usbmodem11301
```

`dependencies-mbed.lock.json` preserves the original lock, core 4.6.0, and
compiler configuration. The guarded Mbed firmware remains in the same source
files. Its artifacts stay in `.build/artifacts/`. Reflashing the Mbed application
replaces the Zephyr loader/application above the original board bootloader;
enter USB bootloader mode manually if the automatic reset fails. `main` also
retains the complete pre-migration source. No branch merge or push is required.

CI now builds both core selections and runs the shared regression suite plus
the Zephyr startup fixture. Local verification is recorded below; remote CI is
not claimed unless it has actually run.

## Validation on 2026-10-22

The repository was clean before work. `feature/zephyr-port` was created before
source changes; `main` was left intact. No merge or push was performed.

| Check | Observed result |
| --- | --- |
| Zephyr build | Pass; 206,032 bytes sketch flash / 557,056 limit; 118,096 bytes sketch RAM / 131,072 limit |
| Mbed regression build | Pass; original footprint retained: 320,088 bytes flash, 188,392 bytes static RAM |
| Host regression suite | Pass; 16 checks, including 10 build-tooling unittest cases and both RTOS startup backends; ASan/UBSan where applicable |
| Source formatting | `make format-check` passed |
| First upload | Arduino CLI/bossac installed the 275,720-byte official loader and 206,032-byte packaged sketch; live Zephyr USB and KWS operation followed |
| Normal capture | 60 seconds, 31 records; about 15,999.7 samples/second; no reported dropped samples, missed buffers, inference errors, event drops, stale scores, or notification errors |
| Spoken keyword | 16 cumulative confirmed actions in telemetry; user independently confirmed the LED toggles on “yes” |
| Monitor closed/reopened | Audio and inference counters advanced across the disconnected interval with no reported losses |
| Control stall | 1.5-second injected stall; 25-second capture; four expected event drops and nine stale events rejected, with no reported audio loss or inference errors |
| Normal restoration | Normal image uploaded again and passed a further 30-second capture; no stress flag in staged firmware |
| Tensor allocation | Startup printed `tensor_arena_used=12500` with the unchanged model |

The normal run ended at 2,343,424 captured samples and 1,137 inferences. Its
cumulative DSP timing was 2.217 ms average / 5.249 ms observed maximum;
inference was 54.994 / 55.054 ms. The audio ring high-water mark was 768 of 8192
samples and the maximum callback gap was 16.022 ms. These are observed timings,
not worst-case guarantees. The restored run independently reached 492,032
samples and 233 inferences without reported errors.

The compiler warns about limited memory: 12,976 bytes remain in the sketch RAM
region after fixed allocations, **including** both application stacks and the
64 KiB arena. This remainder is not a measurement of the loader's system heap,
main/interrupt stacks, or total device free RAM. The reserved arena has ample
space relative to its measured 12,500-byte use, but its unused capacity is still
reserved. CMSIS-DSP's architecture metadata, TFLite's source fallback in place of
a missing precompiled archive, and upstream deprecation warnings remain visible.

Evidence is checked in as [normal telemetry](zephyr-normal.json),
[stress telemetry](zephyr-stress.json), [restored telemetry](zephyr-restored.json),
and the [validation summary with artifact hash](zephyr-validation.json).
Detailed compiler, upload, serial logs, and exact build provenance remain under
`.build/zephyr-*.log` and `.build/zephyr-artifacts/build-manifest.json` locally.
The restored packaged normal image has SHA-256
`f22f4a341ac33cc33df7cc613cbf717b60734c514ca114b263a4833db5985928`.

Remaining manual validation: a cold power-cycle and extended soak, calibrated
recognition/false-positive testing in varied acoustic conditions, and stack
headroom/system heap measurements if needed. Stock-loader stack watermark
support is unavailable; no measurement is claimed. Remote CI has been
configured for both cores but was not run. The Mbed rollback build was compiled,
not flashed during this migration.

## Official references

- [Arduino Core on Zephyr 1.0.0 release](https://github.com/arduino/ArduinoCore-zephyr/releases/tag/1.0.0)
- [Installation, automatic loader updates, and troubleshooting](https://github.com/arduino/ArduinoCore-zephyr/blob/1.0.0/README.md)
- [Nano board and static link configuration](https://github.com/arduino/ArduinoCore-zephyr/blob/1.0.0/boards.txt)
- [Bundled PDM driver](https://github.com/arduino/ArduinoCore-zephyr/blob/1.0.0/libraries/PDM/src/PDM.cpp)
