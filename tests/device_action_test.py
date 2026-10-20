"""Compile actual firmware with the device C++ fixtures."""

from pathlib import Path
import shutil
import subprocess
import tempfile

with tempfile.TemporaryDirectory(prefix="kws-device-") as directory:
    root = Path(directory)
    shutil.copytree(Path(__file__).parent / "fixtures" / "device", root, dirs_exist_ok=True)
    subprocess.run(
        [
            "c++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I" + directory,
            "-Ifirmware",
            "firmware/device_control.cpp",
            str(root / "test.cpp"),
            "-o",
            str(root / "test"),
        ],
        check=True,
    )
    subprocess.run([str(root / "test")], check=True)
