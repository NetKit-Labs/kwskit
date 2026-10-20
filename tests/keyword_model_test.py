"""Exercise the actual model adapter with controlled TFLite tensors and failures."""

from pathlib import Path
import subprocess
import tempfile

with tempfile.TemporaryDirectory(prefix="kws-model-") as directory:
    root = Path(directory)
    for name in (
        "micro/micro_error_reporter.h",
        "micro/micro_interpreter.h",
        "micro/micro_mutable_op_resolver.h",
        "schema/schema_generated.h",
    ):
        header = root / "tensorflow/lite" / name
        header.parent.mkdir(parents=True, exist_ok=True)
        header.write_text('#pragma once\n#include "TensorFlowLite.h"\n')
    executable = root / "test"
    subprocess.run(
        [
            "c++",
            "-std=c++20",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-fsanitize=address,undefined,float-cast-overflow",
            "-I" + directory,
            "-Itests/fixtures/model",
            "-Ifirmware",
            "firmware/keyword_model.cpp",
            "tests/fixtures/model/test.cpp",
            "-o",
            str(executable),
        ],
        check=True,
    )
    for scenario in (
        "normal",
        "schema",
        "registration",
        "allocation",
        "null-input",
        "null-output",
        "input-type",
        "output-type",
        "input-size",
        "output-size",
        "input-data",
        "output-data",
        "input-scale",
        "output-scale",
        "input-zero",
        "output-zero",
    ):
        subprocess.run([str(executable), scenario], check=True)
