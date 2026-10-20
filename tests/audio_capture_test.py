"""Exercise the actual PDM receiver, callback routing, and ring with a deterministic driver fake."""

from pathlib import Path
import subprocess
import tempfile

with tempfile.TemporaryDirectory(prefix="kws-capture-") as directory:
    executable = Path(directory) / "test"
    subprocess.run(
        [
            "c++",
            "-std=c++20",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-fsanitize=address,undefined",
            "-Itests/fixtures/capture",
            "-Ifirmware",
            "firmware/audio_capture.cpp",
            "tests/fixtures/capture/test.cpp",
            "-o",
            str(executable),
        ],
        check=True,
    )
    subprocess.run([str(executable)], check=True)
