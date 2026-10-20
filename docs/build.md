# Command-line build and CI

## Fresh checkout

Install GNU Make, Python 3.9 or newer, and a host C++ compiler (`c++`) supporting
AddressSanitizer and UndefinedBehaviorSanitizer. macOS Command Line Tools and
Ubuntu 24.04's default C++ compiler are suitable host environments. Firmware
cross-compilation uses the downloaded Arm toolchain, not the host C++ compiler.
On Apple Silicon, the core supplies an Intel macOS compiler, so Rosetta 2 must
already be available to run it. The Arduino CLI itself is native ARM64.

## Clone, test, build, and upload

Start in the directory where you want the checkout. Git is required to clone:

```sh
git clone https://github.com/jameslavrenz/KWS_Arm_CortexM4F.git
cd KWS_Arm_CortexM4F
make setup
make test
make build
```

Setup uses Python's standard library and requires HTTPS access to GitHub and
Arduino's download servers. Allow roughly 1-2 GB for core sources, compiler,
libraries, objects, and downloads. Installation is project local; nothing is
installed into the IDE's sketchbook, global package directory, or executable
path. A Nano is not required for testing or compilation.

A successful build prints flash/static RAM usage. Generated firmware is in:

```text
.build/artifacts/KWS_Arm_CortexM4F.ino.elf
.build/artifacts/KWS_Arm_CortexM4F.ino.bin
.build/artifacts/KWS_Arm_CortexM4F.ino.hex
.build/artifacts/build-manifest.json
```

The checked build used 328,880 bytes flash and 194,112 bytes static RAM. Preserve
the manifest beside the binaries when comparing builds. The source remains in
`firmware/`; do not edit the generated sketch under `.build/`.

To upload, connect the Nano using a USB data cable, then identify its port:

```sh
make boards
# macOS example (substitute the port printed for your board):
make flash PORT=/dev/cu.usbmodem11201
# Linux example:
make flash PORT=/dev/ttyACM0
```

`make flash` builds before uploading and requires an explicit `PORT`. Without it,
the command fails before compiling or touching a device. Close any serial monitor
that holds the port open. On Linux, the user also needs serial-device permissions
supplied by the host configuration. Firmware starts after upload and continues
running while powered, independently of the host terminal.

For microphone/LED verification, create a host environment with pyserial:

```sh
python3 -m venv .build/hardware-venv
.build/hardware-venv/bin/python -m pip install pyserial==3.5
.build/hardware-venv/bin/python tests/hardware_capture.py --port /dev/cu.usbmodem11201 --seconds 60
```

Use the board's actual port on either operating system. Say "yes" with pauses
of about three seconds and observe the built-in LED. The capture checks audio
and inference progression and reported drops/errors; physical action and speech
correspondence require observation. Ending the capture does not stop firmware.

## Locked inputs and provenance

| Input | Pin / verification |
|---|---|
| Arduino CLI | 1.5.1; SHA-256 for each supported macOS/Linux host archive |
| Arduino board core | `arduino:mbed_nano@4.6.0`; package manager checksums |
| Arm compiler | `arm-none-eabi-gcc@7-2017q4`, selected by that core's package dependencies |
| CMSIS-DSP | Arduino_CMSIS-DSP 5.7.0; SHA-256-verified official library ZIP |
| TFLite Micro Arduino sources | `7948b85e020470a30a83bf00e10fc43b486bfcba`; SHA-256-verified upstream ZIP |
| TFLite compatibility headers | Reviewed, tracked files under `build-support/tflm-compat/` |
| Firmware, model and mel coefficients | Tracked files under `firmware/`; hashes included with outputs |

The original installed TFLite library reports `2.4.0-ALPHA`, but that version label
alone does not identify the source. The lock therefore records its exact Git
revision. Its three additional compatibility headers had previously existed only
in the developer's local library; setup now installs the tracked copies. See
[compatibility provenance](../build-support/tflm-compat/README.md).

`dependencies.lock.json` is the single source of dependency versions and download
checksums. Setup verifies CLI/library archives before extracting them. Arduino
CLI verifies the platform and tool archives against the official package index.
The core archive also has an explicit SHA-256 pin. Setup verifies every regular
file against that archive and restores any missing files: a clean local CLI
installation omitted CMSIS/other headers that were present in the official
archive. This restores original upstream files without patching their content.
Build checks the recorded core file hashes as well as library hashes.
Setup records a per-file library manifest; build rejects missing/modified library
files and changed compatibility overlays until setup is rerun.

`scripts/project.py` passes an explicit local Arduino CLI configuration and removes
ambient `ARDUINO_*` environment settings from child CLI calls. It verifies the
installed CLI/core versions, stages only maintained firmware source files into a
correctly named sketch directory, and compiles with fixed board selection. The
small `.ino` remains a board-framework entry point; there is no IDE dependency.

Outputs in `.build/artifacts/` include the ELF/BIN/HEX files generated by the core
and `build-manifest.json` with dependency pins, source/header hashes, and host
identity. Builds do not promise byte-identical binaries across host operating
systems or paths. The host compiler/Python patch versions and CLI-discovered USB
tools are not firmware compiler pins. CI uses Ubuntu 24.04; its runner image can
receive updates. Preserve manifests when comparing artifacts.

## GitHub Actions

`.github/workflows/ci.yml` has independent host-test and firmware-build jobs,
triggered by pushes, pull requests, or manual dispatch. Both use the Make targets
above. Workflow actions are pinned by commit, permissions are read-only, and the
firmware job uploads compiled outputs plus the build manifest. CI does not flash
hardware or claim to validate speech accuracy.

The first run downloads everything into a fresh `.build/`; the workflow does not
rely on a developer's installed Arduino libraries or a prepopulated dependency
cache. To change dependencies, update the lock, rerun setup and all checks, then
record any measured firmware changes before updating performance claims.

## Commands

| Command | Purpose |
|---|---|
| `make setup` | Install/restore locked dependencies (network required) |
| `make test` | Host unit/integration tests; no board or Arduino setup required |
| `make build JOBS=2` | Verify dependencies and cross-compile |
| `make boards` | List connected boards using the local CLI |
| `make flash PORT=<port>` | Build then upload to an explicit serial port |

Build, test, and flash do not modify maintained source files. Generated files live
under ignored `.build/`. Existing IDE workflows remain optional; the CLI setup
also captures the compatibility dependencies those workflows must supply.


## Troubleshooting

| Symptom | Action |
|---|---|
| `make`, `python3`, or `c++` missing | Install the host prerequisites; macOS Command Line Tools supply Make/Clang, and Linux needs a C++ development toolchain. Confirm Python is 3.9+. |
| Missing/modified dependency or changed lock | Rerun `make setup` before `make build`; setup restores the pinned sources. |
| SHA-256 mismatch or interrupted download | Check the network and rerun `make setup`. Do not bypass checksum verification. |
| Apple Silicon cannot execute the Arm compiler | Ensure Rosetta 2 is installed; the pinned compiler is an Intel macOS executable. |
| Serial port absent or busy | Check the USB data cable, close other monitors, and rerun `make boards`. |
| Upload requires bootloader recovery | Double-press the Nano's reset button, rerun `make boards`, and use the bootloader port shown for the retry. |
| Upstream warnings / unavailable TFLite precompiled library | The pinned library builds from source; inspect the final build exit status rather than treating these notices as failures. |

## PDF guide

`docs/README.pdf` is generated from the current project README. To regenerate it
without installing packages globally:

```sh
python3 -m venv .build/docs-venv
.build/docs-venv/bin/python -m pip install -r requirements-docs.txt
.build/docs-venv/bin/python scripts/export_docs.py
```

The PDF contains the same overview, architecture, measurements, and build entry
points as the README. This Markdown build guide remains the detailed procedure.
The export command writes `docs/README.pdf` and does not build or flash firmware.
