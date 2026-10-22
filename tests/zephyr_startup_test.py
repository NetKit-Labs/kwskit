"""Compile the native Zephyr backend against deterministic lifecycle fixtures."""

from pathlib import Path
import subprocess
import tempfile

with tempfile.TemporaryDirectory(prefix="kws-zephyr-") as directory:
    executable = str(Path(directory) / "test")
    subprocess.run(
        [
            "c++",
            "-std=c++20",
            "-DARDUINO_ARCH_ZEPHYR",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-fsanitize=address,undefined",
            "-Itests/fixtures/zephyr",
            "-Ifirmware",
            "-Itests/fixtures/support",
            "firmware/kws_pipeline.cpp",
            "tests/fixtures/zephyr/test.cpp",
            "-o",
            executable,
        ],
        check=True,
    )
    subprocess.run([executable], check=True)
