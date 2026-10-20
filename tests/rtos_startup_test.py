"""Compile actual firmware with the rtos C++ fixtures."""

from pathlib import Path
import shutil
import subprocess
import tempfile

with tempfile.TemporaryDirectory(prefix="kws-rtos-") as directory:
    root = Path(directory)
    shutil.copytree(Path(__file__).parent / "fixtures" / "rtos", root, dirs_exist_ok=True)
    subprocess.run(
        [
            "c++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-fsanitize=address,undefined",
            "-I" + directory,
            "-Ifirmware",
            "-Itests/fixtures/support",
            "firmware/kws_pipeline.cpp",
            str(root / "test.cpp"),
            "-o",
            str(root / "test"),
        ],
        check=True,
    )
    subprocess.run([str(root / "test")], check=True)
