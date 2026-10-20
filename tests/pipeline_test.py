"""Compile actual firmware with the pipeline C++ fixtures."""

from pathlib import Path
import shutil
import subprocess
import tempfile

with tempfile.TemporaryDirectory(prefix="kws-pipeline-") as directory:
    root = Path(directory)
    shutil.copytree(Path(__file__).parent / "fixtures" / "pipeline", root, dirs_exist_ok=True)
    subprocess.run(
        [
            "c++",
            "-std=c++11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-fsanitize=address,undefined",
            "-I" + directory,
            "-Ifirmware",
            "-Itests/fixtures/support",
            "firmware/audio_pipeline.cpp",
            str(root / "test.cpp"),
            "-o",
            str(root / "test"),
        ],
        check=True,
    )
    subprocess.run([str(root / "test")], check=True)
