"""Compile valid public buffer calls and deliberately invalid ownership/extent probes."""

from pathlib import Path
import subprocess

probes = sorted((Path(__file__).parent / "fixtures" / "contracts").glob("*.cpp"))
assert any(p.name.startswith("accept_") for p in probes)
assert any(p.name.startswith("reject_") for p in probes)
for source in probes:
    assert source.name.startswith(("accept_", "reject_")), source
    should_compile = source.name.startswith("accept_")
    result = subprocess.run(
        [
            "c++",
            "-std=c++11",
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
    assert (
        result.returncode == 0
    ) == should_compile, (
        f"{source.name}: expected compile success={should_compile}\n{result.stderr}"
    )
print(f"PASS: {len(probes)} buffer/ownership compile contracts")
