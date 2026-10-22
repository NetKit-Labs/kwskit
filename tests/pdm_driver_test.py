"""Fault-test the actual pinned/staged PDM worker; requires make setup, no board."""

import importlib.util
from pathlib import Path
import re
import resource
import subprocess
import tempfile

spec = importlib.util.spec_from_file_location(
    "project", Path(__file__).resolve().parents[1] / "scripts/project.py"
)
project = importlib.util.module_from_spec(spec)
spec.loader.exec_module(project)
project.verify_core()
staged = project.stage_pdm_library()


def no_core_dump():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


with tempfile.TemporaryDirectory(prefix="kws-pdm-driver-") as directory:
    root = Path(directory)
    for source, patched in ((project.core_directory() / "libraries/PDM", False), (staged, True)):
        text = (source / "src/PDM.cpp").read_text()
        worker = re.search(r"^void pdm_thread\([^\n]+\) \{\n.*?^\}", text, re.M | re.S)
        assert worker, "Pinned PDM worker boundaries changed"
        (root / "pdm_thread.inc").write_text(worker.group() + "\n")
        executable = root / "test"
        subprocess.run(
            [
                "c++",
                "-std=c++20",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-fsanitize=address,undefined",
                "-I" + directory,
                "tests/fixtures/pdm_driver/test.cpp",
                "-o",
                str(executable),
            ],
            check=True,
            cwd=project.ROOT,
        )
        result = subprocess.run(
            [str(executable)], capture_output=True, text=True, preexec_fn=no_core_dump
        )
        if patched:
            assert result.returncode == 0, result.stderr
        else:
            assert result.returncode != 0 and "sleepCalls == errorReads" in result.stderr, (
                "Fault test must detect the original tight retry loop: " + result.stderr
            )
print("PASS: actual PDM worker yields on transient/persistent errors and handles queue pressure")
