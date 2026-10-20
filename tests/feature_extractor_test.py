"""Check actual extractor state and frozen pre-refactor feature vectors on a scalar host DSP backend."""

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
    # Captured from a2c3c0d using this exact PCM sequence and scalar host backend.
    assert hashlib.sha256(features).hexdigest() == (
        "1e4480b7c55f89716fa078a30dcc7fb449670c296205982a47fd5bdc0eb7bbde"
    ), "Quantized features changed from the pre-refactor reference"
