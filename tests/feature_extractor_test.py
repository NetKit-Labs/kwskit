"""Check actual extractor state and frozen periodic-Hann feature vectors on a scalar host DSP backend."""

import hashlib
from pathlib import Path
import subprocess
import tempfile

with tempfile.TemporaryDirectory(prefix="kws-features-") as directory:
    executable = Path(directory) / "test"
    subprocess.run(
        [
            "c++",
            "-std=c++11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-fsanitize=address,undefined",
            "-Itests/fixtures/support",
            "-Ifirmware",
            "firmware/feature_extractor.cpp",
            "tests/fixtures/support/arm_math.cpp",
            "tests/fixtures/features/test.cpp",
            "-o",
            str(executable),
        ],
        check=True,
    )
    features = subprocess.check_output([str(executable)])
    assert len(features) == 84180, len(features)
    # Reference: procedural a2c3c0d extractor with periodic Hann, using this PCM and host backend.
    assert hashlib.sha256(features).hexdigest() == (
        "c14a17d8294d72bc45082f4bd525fd15a93f77390b5e05658a9e31f969327d81"
    ), "Quantized features changed from the periodic-Hann reference"
