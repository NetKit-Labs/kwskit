#!/usr/bin/env python3
"""Hardware-only control-backpressure test; always restore normal firmware."""
import argparse
import json
import subprocess
import sys
import time
import project


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--seconds", type=int, default=20)
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--output", default="/tmp/kws-rtos-stress.json")
    args = parser.parse_args()
    project.configure()
    project.build(args.jobs)
    stress_output = project.BUILD / "stress-artifacts"
    try:
        header = project.SKETCH / "kws_config.h"
        header.write_text(header.read_text() + "\n#define KWS_RTOS_STRESS_TEST 1\n")
        project.cli(
            "compile",
            "--fqbn",
            project.LOCK["board"]["fqbn"],
            "--jobs",
            args.jobs,
            "--warnings",
            "default",
            "--build-path",
            project.BUILD / "objects",
            "--output-dir",
            stress_output,
            project.SKETCH,
        )
        (stress_output / "stress-manifest.json").write_text(
            json.dumps(
                {
                    "test_only": True,
                    "control_stall_ms": 1500,
                    "firmware": project.tree_manifest(project.SKETCH),
                    "dependencies": project.LOCK,
                },
                indent=2,
            )
            + "\n"
        )
        project.cli(
            "upload",
            "--fqbn",
            project.LOCK["board"]["fqbn"],
            "--port",
            args.port,
            "--input-dir",
            stress_output,
        )
        # USB reenumeration after the bootloader can lag the upload process.
        time.sleep(3)
        subprocess.run(
            [
                sys.executable,
                str(project.ROOT / "tests/hardware_capture.py"),
                "--port",
                args.port,
                "--seconds",
                str(args.seconds),
                "--expect-event-drops",
                "--output",
                args.output,
            ],
            check=True,
        )
    finally:
        print("Restoring normal firmware (stress flag absent).", flush=True)
        project.build(args.jobs)
        project.cli(
            "upload",
            "--fqbn",
            project.LOCK["board"]["fqbn"],
            "--port",
            args.port,
            "--input-dir",
            project.OUTPUT,
        )


if __name__ == "__main__":
    main()
