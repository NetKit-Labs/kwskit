"""Compile valid public buffer calls and deliberately invalid ownership/extent probes."""

from pathlib import Path
import subprocess

probes = sorted((Path(__file__).parent / "fixtures" / "contracts").glob("*.cpp"))
assert any(p.name.startswith("accept_") for p in probes)
assert any(p.name.startswith("reject_") for p in probes)


def compile_probe(source, standard="c++20"):
    return subprocess.run(
        [
            "c++",
            "-std=" + standard,
            "-Wall",
            "-Wextra",
            "-Werror",
            "-fsyntax-only",
            "-Itests/fixtures/capture",
            "-Itests/fixtures/support",
            "-Ifirmware",
            str(source),
        ],
        capture_output=True,
        text=True,
    )


for source in probes:
    assert source.name.startswith(("accept_", "reject_")), source
    should_compile = source.name.startswith("accept_")
    result = compile_probe(source)
    assert (
        result.returncode == 0
    ) == should_compile, (
        f"{source.name}: expected compile success={should_compile}\n{result.stderr}"
    )
print(f"PASS: {len(probes)} buffer/ownership compile contracts")

for standard in ("c++11", "c++14", "c++17"):
    result = compile_probe(
        Path(__file__).parent / "fixtures/contracts/accept_valid_buffers.cpp", standard
    )
    assert result.returncode != 0 and "Project code requires C++20" in result.stderr, result.stderr
print("PASS: C++11/C++14/C++17 rejected by the project language guard")
