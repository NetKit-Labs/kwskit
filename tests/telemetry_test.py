"""Exercise actual Zephyr Serial telemetry under disconnection and backpressure."""

from pathlib import Path
import subprocess
import tempfile

with tempfile.TemporaryDirectory(prefix="kws-telemetry-") as directory:
    executable = str(Path(directory) / "test")
    subprocess.run(
        [
            "c++",
            "-std=c++20",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-fsanitize=address,undefined",
            "-Itests/fixtures/telemetry",
            "-Ifirmware",
            "firmware/telemetry.cpp",
            "tests/fixtures/telemetry/test.cpp",
            "-o",
            executable,
        ],
        check=True,
    )
    subprocess.run([executable], check=True)
