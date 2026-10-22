# Source language and formatting

Maintained firmware, the host model inspector and native tests require C++20.
Host tests use `-std=c++20`; firmware uses `-std=gnu++20` with Arduino's official
GCC 14.3.0 toolchain. `cpp_standard.h` rejects older modes and preloads `<chrono>`
before Arduino's `abs` macro. The `.ino` entry point is compiled as C++ and only
calls application startup/poll. No project-owned `.c` units need conversion.
Downloaded core/DSP/inference dependencies retain their own C/C++ recipes.

## References, boundaries and includes

Use references for required existing objects and array references for fixed-size
buffer interfaces. Keep pointers for optional/uninitialized objects and raw
platform/DSP/USB interfaces. Do not replace a meaningful null state with a dummy
reference. [C++ ownership](cpp-idioms.md) explains the lifetime rules.

Include groups are matching header, standard C++ headers, platform/library
headers, then other project headers. Each group is sorted with a blank line
between groups. The intentional `cpp_standard.h`/`kws_config.h` prelude remains
before dependent platform headers; its macros/include order are functional.
The grouping is configured in [.clang-format](../.clang-format).

## Four-space policy and tools

C++ uses four spaces, a 100-column target, namespace indentation and blank lines
between definitions. Python and notebook cells use Black with four-space blocks,
100-column target and Python 3.9 compatibility. Shell/YAML blocks use four spaces;
Make recipes require tabs. [.editorconfig](../.editorconfig) supplies defaults.
Captured JSON evidence retains its presentation.

```sh
python3 -m venv .build/style-venv
.build/style-venv/bin/python -m pip install -r requirements-style.txt
make format
make format-check
```

[requirements-style.txt](../requirements-style.txt) pins clang-format 18.1.8 and
Black 24.10.0 with notebook support. These optional tools are project-local;
build and host tests do not need them. `CLANG_FORMAT` and `BLACK` can override
executable paths, but use the pins to match CI.

Targets cover maintained firmware, native tests, fake Arduino/PDM/Zephyr/IRQ/DSP
interfaces (including nested fixture headers), the model inspector, compatibility
headers, Python harnesses/tools and notebook code cells. Downloaded libraries
and generated build artifacts are excluded. Generated model/mel tables are
maintained as data; do not change their values for presentation. Refresh setup
when tracked compatibility headers change.

The CI source-style job runs `make format-check`. Independent host and Zephyr
firmware jobs verify behavior and compilation. Formatting alone does not prove
runtime equivalence; [validation](validation.md) identifies actual checks.
