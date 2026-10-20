# Source language and formatting

## Language audit

The maintained native firmware, host model inspector, and native unit/regression
tests require C++17 or later. Host tests and the model inspector use `-std=c++17`;
the project overrides the pinned Arduino Mbed core default with `-std=gnu++17`
for firmware. `cpp_standard.h` rejects older modes. The build preincludes that
header to load `<chrono>` before Arduino defines its conflicting `abs` macro.
There are no project-owned `.c` translation units to convert.
Headers are included by C++ sources; the Arduino `.ino` is compiled as C++ and
only delegates startup/idle handling. C-compatible declarations and C library
APIs called from a C++ translation unit do not change that unit's language.

The project also uses Python for build/test/document tools and the training
notebook, shell for the host test runner, and Make/YAML for build/CI configuration.
The downloaded Arduino/Mbed board core, CMSIS, and TFLite dependencies include
upstream C/C++ implementations. Those sources and C interfaces remain intact;
compiled C++ sources receive the documented language/include flags. The complete
dependency stack also contains C and a precompiled Mbed archive.

The [C++ idioms review](cpp-idioms.md) records typed buffer contracts, RAII
critical sections/lifetimes, noncopyable owners, and the native-source review.

## References and pointer boundaries

Use references for required existing objects and array references for owned buffer
interfaces. The ring's `pop` takes a `std::array` reference and optionally a count,
rejecting counts larger than its destination before consuming samples. Borrowed
capture/extractor/model dependencies and model-inspector graph objects also use
references. Keep pointers when null is meaningful (uninitialized runtime tensors,
optional FlatBuffer fields, inactive capture ownership) and for the PDM/CMSIS/USB
API signatures and raw byte buffers they require. A reference is not a replacement
for an optional value, a reseatable handle, or an array extent.

The [C++17 update](cpp17-modernization.md) records the modern library facilities,
compile-mode regression checks, and hardware verification. C++20 is not needed
by the current implementation or selected by the pinned firmware toolchain.

## Four-space policy

- C++: four spaces per indentation level, including namespaces, with formatter
  alignment of wrapped expressions where applicable;
  100-column formatting, with include order preserved. See [.clang-format](../.clang-format).
- Python and notebook code cells: four-space blocks and Black's standard
  continuation formatting, with a 100-column target and Python 3.9 compatibility.
  See [pyproject.toml](../pyproject.toml).
- Shell blocks and maintained YAML configuration: four-space indentation.
- Makefile recipes: tabs required by Make syntax.
- [.editorconfig](../.editorconfig) supplies editor defaults. Captured telemetry
  JSON is evidence data and retains its recorded presentation.

The fake Arduino/CMSIS interfaces and test programs are ordinary `.h`/`.cpp`
fixtures in `tests/fixtures/device`, `tests/fixtures/pipeline`, and
`tests/fixtures/rtos`, plus capture/features fixtures and scalar DSP support.
Formatting checks include them directly. Harnesses compile fixtures either from
temporary copies or their maintained paths, with actual firmware modules.

## Install and run formatters

From the repository root:

```sh
python3 -m venv .build/style-venv
.build/style-venv/bin/python -m pip install -r requirements-style.txt
make format
make format-check
```

[requirements-style.txt](../requirements-style.txt) pins clang-format 18.1.8
and Black 24.10.0 with notebook support. Installation is project local; the normal
firmware build and host regression suite do not require these optional tools.
`CLANG_FORMAT` and `BLACK` can override executable paths when needed; use the
pinned versions for results matching CI. `make format` changes maintained code;
`make format-check` reports drift without rewriting files.

The formatter targets cover `firmware/` C++ headers/sources/entry point, native
tests and fixtures, `scripts/inspect_model.cpp`, the tracked TFLite compatibility
headers, Python tools/harnesses, and training notebook code cells. They do not
rewrite downloaded libraries, telemetry captures, or unrelated local copies.
Markdown examples and YAML/shell configuration are maintained with four-space
indentation where the syntax uses indentation.

The source-style CI job installs the pinned formatters and runs
`make format-check` on pushes and pull requests. Separate jobs run the full host
regression suite and a clean Nano firmware build, including artifact upload.

The [C++ ownership guide](cpp-refactor.md) describes the subsequent object refactor:
stateful modules now use private members, fixed-size arrays, const accessors, and
explicit dependencies. Namespaces continue to group stateless entry points and
implementation-only types; they are normal C++, not a compiler workaround.

## Formatting-only validation (before the object refactor)

On October 20, 2026, the maintained native-source audit found no `.c` files.
`make format-check` passed for the formatted C++/Python/notebook sources.
All eleven entries in `make test` passed after the test fixtures were extracted.
The notebook's Python code had equivalent syntax trees before/after formatting,
and its saved training outputs were preserved. The model bytes and mel coefficient
values are checked separately from their source-header layout.

`make build JOBS=4` passed with the same 331,440-byte flash and 209,320-byte
static RAM footprint. The final application BIN is byte-for-byte identical to
the pre-format build (332,036 bytes, including binary image layout). Its SHA-256 is:

```text
f0467c781f1705ba5b2d9433975391cc444c71a5aedc49294fdd9e10e999d500
```

The build manifest matches the maintained formatted firmware. Model bytes, mel
coefficient values, dependency pins, and the model inspector's metadata also
matched their pre-format equivalents. The full regression suite and formatter
checks passed again after the final formatting pass. The existing hardware
records describe the same application binary; this formatting validation did
not add a new physical microphone/LED session.

The subsequent semantic refactor has its own [verification record](cpp-refactor.md#verification).
Its binary is different; the byte-identity statement above applies only to the
earlier formatting change.
