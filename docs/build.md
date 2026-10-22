# Zephyr build and continuous integration

## Prerequisites and fresh checkout

Use Git, GNU Make, Python 3.9+, and a C++20 host compiler with AddressSanitizer
and UndefinedBehaviorSanitizer. macOS Command Line Tools and Ubuntu 24.04 work.
Initial setup needs HTTPS access to GitHub and Arduino downloads and several GB
of workspace storage. Firmware builds use Arduino's packaged cross-compiler.

```sh
git clone https://github.com/NetKit-Labs/KWS_Arm_CortexM4F.git
cd KWS_Arm_CortexM4F
make setup
make test
make build JOBS=4
make boards
make flash PORT=/dev/cu.usbmodem11301 JOBS=4
```

Choose the port printed for your Nano. On Linux it commonly resembles
`/dev/ttyACM0` and requires the host's serial-device permissions. Close competing
monitors before upload. Apple Silicon needs Rosetta 2 for the core's Intel
`bossac` uploader; the CLI and compiler themselves are native ARM64.

## Single supported target and locked inputs

| Input | Selection |
| --- | --- |
| Board/FQBN | `arduino:zephyr_main:nano33ble:link_mode=static` |
| Platform | `arduino:zephyr_main@1.0.0` |
| Arduino CLI | 1.5.1, host archive SHA-256 pinned |
| Official compiler | `zephyr:arm-zephyr-eabi@1.0.1`, GCC 14.3.0 |
| Feature processing | Arduino_CMSIS-DSP 5.7.0, verified source ZIP |
| Inference | Arduino_TensorFlowLite 2.4.0-ALPHA, exact source revision and ZIP hash pinned |
| Compatibility declarations | Tracked headers under `build-support/tflm-compat/` |

[dependencies.lock.json](../dependencies.lock.json) identifies the inputs. The
version label on the inference library alone does not identify its source;
the lock includes its exact revision. The core comes from Arduino's standard
package index; no development index, separate SDK, west environment or
compiler override is required.

Setup installs everything under `.build/` using an explicit CLI configuration.
It does not change global Arduino libraries or the user's sketchbook. CLI and
library archives are checked against pinned SHA-256 values. Arduino CLI checks
core/tool package archives against its official index; setup also checks the
pinned core archive and records/restores every regular file. Before building,
the script verifies CLI/core/compiler versions and core/library file manifests.
Compiler binaries are provided by the package manager; the project checks their
reported version rather than claiming a per-file compiler integrity manifest.

The inference dependency's unrelated I2S/peripheral examples and serial-test
constructor are excluded at staging. Its runtime and CMSIS-NN sources are
retained. [Compatibility notes](../build-support/tflm-compat/README.md) explain
the three tracked declarations. Library transformations participate in the
installed manifest. Rerun setup after changing those inputs.

Each build also stages a [PDM driver compatibility patch](../build-support/pdm/README.md)
under `.build/pdm-realtime/PDM` and selects it with `--library`. It adds a 1 ms
sleep after a failed device read, preventing a tight high-priority retry loop.
Exact source matching rejects an unexpected upstream change. The installed
official core remains unmodified; normal/stress manifests record the staged
driver hashes. No global Arduino library is replaced.

## Compilation and output

`firmware/` is authoritative. `scripts/project.py` stages maintained `.ino`,
`.cpp` and `.h` files under `.build/zephyr-sketch/KWS_Arm_CortexM4F/`, preserving
unchanged file timestamps. It strips ambient `ARDUINO_*` settings from CLI child
processes and selects the locked FQBN. Arduino's normal recipes compile/link the
sketch; this is not an independent startup or linker build.

C++ sources receive `-std=gnu++20 -fno-exceptions -fno-rtti -Werror=vla` and a
forced include of `cpp_standard.h`.
That header rejects older modes, loads `<chrono>` before Arduino's conflicting
`abs` macro and strips inference error strings from the blocking logger.
Imported C sources use the core's C recipe. The official core selects Cortex-M4F
hard-float machine/ABI flags. All C++ sources also receive
`-DPDM_NUMBER_OF_SAMPLES=256 -DPDM_THREAD_PRIORITY=3`, configuring the bundled
PDM driver consistently with the application.

```text
.build/zephyr-objects/                         compiler working files
.build/zephyr-artifacts/KWS_Arm_CortexM4F.ino.elf
.build/zephyr-artifacts/KWS_Arm_CortexM4F.ino.bin
.build/zephyr-artifacts/KWS_Arm_CortexM4F.ino.hex
.build/zephyr-artifacts/KWS_Arm_CortexM4F.ino.bin-zsk.bin
.build/zephyr-artifacts/build-manifest.json
.build/zephyr-artifacts/realtime-audit.json
```

Use `make flash`; Arduino's static-mode upload recipe selects the packaged
`bin-zsk.bin`, not an arbitrary intermediate ELF/BIN/HEX. The manifest records
staged firmware hashes, dependency pins, compatibility headers, build-script
hash, language/safety flags and host identity. After compilation, the official
`arm-zephyr-eabi-nm` checks all sketch objects for direct heap/deallocation,
exception and out-of-line atomic imports. Placement construction in supplied
storage is allowed. A failure stops the build; `realtime-audit.json` records the
passed imports. This is a direct-reference policy, not a transitive analysis of
core/library/loader internals or a worst-case execution-time proof. Preserve it with artifacts. Byte-identical
outputs across different host systems, paths or future core versions are not
promised. Object/core caches may differ while input manifests remain reviewable.

## Loader and hardware checks

The CLI automatically installs or updates the matching Zephyr loader during
upload, then writes the sketch. The board's original USB bootloader provides
recovery. If automatic 1200-baud reset fails, double-press RESET, run `make boards`
and retry with the bootloader's enumerated port. See [runtime](zephyr-runtime.md)
for static linking and memory limits.

Install PySerial separately for live checks:

```sh
python3 -m venv .build/hardware-venv
.build/hardware-venv/bin/python -m pip install pyserial==3.5
.build/hardware-venv/bin/python tests/hardware_capture.py --port /dev/cu.usbmodem11301 \
    --seconds 60 --allow-unavailable-stack-metrics --output .build/normal.json
.build/hardware-venv/bin/python scripts/rtos_stress.py --port /dev/cu.usbmodem11301 \
    --seconds 25 --output .build/stress.json
```

Observe the physical LED while speaking “yes” separately. The capture checks
telemetry, not labeled speech accuracy. Stress uploads a test-only image and
restores normal firmware in `finally`; check its restoration result.

## Commands and CI

| Command | Purpose |
| --- | --- |
| `make setup` | Install/restore locked tools and libraries |
| `make test-unit` | Isolated actual-module and pure-component checks |
| `make test-regression` / `make test` | Units plus stream, pipeline, build tooling and Zephyr kernel fixtures |
| `make test-pdm-driver` | After setup, fault-test the actual pinned and patched PDM worker without a board |
| `make build JOBS=2` | Verified Zephyr cross-build |
| `make boards` | Discover boards with the local CLI |
| `make flash PORT=<port>` | Build then upload; an explicit port is required |
| `make format` / `make format-check` | Apply/check pinned source formatting |

[GitHub Actions](../.github/workflows/ci.yml) has formatting, documentation, host-tests and a
single Zephyr firmware job. It runs on pushes, pull requests and manual dispatch.
The firmware job installs dependencies from scratch, builds and uploads
`nano33ble-zephyr-firmware` containing outputs and provenance. Actions are pinned
by commit and workflow permissions are read-only. CI does not flash a Nano or
measure acoustic accuracy. [Validation](validation.md) links actual runs.
It also runs `make test-pdm-driver` after setup; this gate needs the pinned core
source and complements the download-free host suite.

## Optional IDE configuration

The CLI/Make workflow is tested. An IDE build must use the same core, dependency
sources, static link mode and C++20/PDM flags. A `platform.local.txt` beside the
IDE core's `platform.txt` can supply:

```text
compiler.cpp.extra_flags=-std=gnu++20 -fno-exceptions -fno-rtti -Werror=vla -include "/absolute/path/to/project/firmware/cpp_standard.h" -DPDM_NUMBER_OF_SAMPLES=256 -DPDM_THREAD_PRIORITY=3
```

Open `firmware/KWS_Arm_CortexM4F.ino`, select the Nano under Arduino Zephyr Boards
and static link mode, and inspect verbose output. There is no `compiler.path`
override. IDE UI behavior has not been independently validated. Core reinstalls
can remove local properties; Make configures them automatically.
For equivalent PDM retry behavior, the IDE must additionally select the staged
PDM library described above. The flags alone do not apply that source patch;
use the Make/CLI path for the complete tested configuration.

## Troubleshooting and PDF generation

For missing/modified dependencies, rerun `make setup`. Do not bypass checksum
failures. For a missing USB port, check the data cable and bootloader mode.
CMSIS-DSP's upstream architecture metadata can cause a compatibility warning;
the actual source is cross-built for this target. The pinned inference library
falls back to source when no matching precompiled archive exists. Upstream
deprecation warnings are visible; inspect the actual build exit status.
The stock loader has no stack-watermark support, so hardware captures use the
explicit `--allow-unavailable-stack-metrics` option and retain all other checks.

```sh
python3 -m venv .build/style-venv
.build/style-venv/bin/python -m pip install -r requirements-style.txt
make format-check
python3 -m venv .build/docs-venv
.build/docs-venv/bin/python -m pip install -r requirements-docs.txt
.build/docs-venv/bin/python scripts/export_docs.py
.build/docs-venv/bin/python scripts/export_docs.py --check
```

The only maintained PDF is `docs/README.pdf`, generated from the root `README.md`.
It does not export `docs/README.md` or the separate topic guides. Edit Markdown
first, regenerate, then commit both source and PDF. The export also writes an
identical ignored copy to `output/pdf/README.pdf`.

The PDF retains the guide's text, tables, commands and links. Its architecture
diagram becomes a connection table generated from every Mermaid node and edge;
unsupported diagram syntax fails instead of dropping information. Typography is
normalized to ASCII and the live CI badge becomes its linked alt text.

The pinned exporter produces deterministic bytes and embeds the root Markdown's
SHA-256 in PDF metadata. `--check` regenerates in memory and fails if the tracked
PDF differs or is missing; it writes no files. The CI documentation job runs this
check, preventing Markdown/exporter changes from leaving a stale PDF. It checks
source/export parity, not whether descriptions still match firmware behavior.
Review the relevant Markdown when code changes, and render and visually check
every PDF page after regenerating.
