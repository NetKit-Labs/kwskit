#!/usr/bin/env python3
"""Pinned, project-local Arduino build. Python standard library only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / ".build"
CORE = os.environ.get("KWS_CORE", "zephyr")
if CORE not in ("mbed", "zephyr"):
    raise RuntimeError("KWS_CORE must be mbed or zephyr")
ZEPHYR = CORE == "zephyr"
LOCK = json.loads(
    (ROOT / ("dependencies.lock.json" if ZEPHYR else "dependencies-mbed.lock.json")).read_text()
)
CLI = BUILD / "bin" / "arduino-cli"
CONFIG = BUILD / "arduino-cli.json"
SKETCH = BUILD / ("zephyr-sketch" if ZEPHYR else "sketch") / "KWS_Arm_CortexM4F"
LIBRARIES = BUILD / "arduino-user" / "libraries"
OUTPUT = BUILD / ("zephyr-artifacts" if ZEPHYR else "artifacts")
CPP_STANDARD = "gnu++20"


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def download(url, expected):
    cache = BUILD / "downloads"
    cache.mkdir(parents=True, exist_ok=True)
    target = cache / expected
    if target.exists() and sha256(target) == expected:
        return target
    print("Downloading " + url, flush=True)
    partial = target.with_suffix(".partial")
    request = urllib.request.Request(url, headers={"User-Agent": "KWS-build"})
    with urllib.request.urlopen(request, timeout=120) as response, partial.open("wb") as output:
        shutil.copyfileobj(response, output)
    if sha256(partial) != expected:
        partial.unlink()
        raise RuntimeError("Checksum mismatch: " + url)
    partial.replace(target)
    return target


def configure():
    BUILD.mkdir(exist_ok=True)
    config = {
        "directories": {
            "data": str(BUILD / "arduino-data"),
            "downloads": str(BUILD / "arduino-downloads"),
            "user": str(BUILD / "arduino-user"),
        },
        "build_cache": {"path": str(BUILD / "cache")},
        "updater": {"enable_notification": False},
    }
    CONFIG.write_text(json.dumps(config, indent=2) + "\n")


def cli(*args, capture=False):
    # Prevent ambient Arduino environment settings from defeating isolation.
    environment = {
        key: value for key, value in os.environ.items() if not key.startswith("ARDUINO_")
    }
    command = [str(CLI), "--config-file", str(CONFIG), *map(str, args)]
    print("+ " + " ".join(command), flush=True)
    return subprocess.run(
        command,
        cwd=ROOT,
        env=environment,
        check=True,
        text=True,
        stdout=subprocess.PIPE if capture else None,
    )


def require_cli():
    if not CLI.exists():
        raise RuntimeError("Run make setup first.")
    version = json.loads(cli("version", "--json", capture=True).stdout)["VersionString"]
    if version != LOCK["arduino_cli"]["version"]:
        raise RuntimeError("Arduino CLI version differs from dependencies.lock.json")


def install_cli():
    key = platform.system() + "-" + platform.machine().lower()
    asset = LOCK["arduino_cli"]["archives"].get(key)
    if not asset:
        raise RuntimeError("Unsupported build host: " + key)
    archive = download(LOCK["arduino_cli"]["base_url"] + asset["file"], asset["sha256"])
    CLI.parent.mkdir(parents=True, exist_ok=True)
    # Extract only the binary; archive paths cannot write elsewhere.
    with tarfile.open(archive) as source:
        members = [
            member
            for member in source.getmembers()
            if Path(member.name).name == "arduino-cli" and member.isfile()
        ]
        if len(members) != 1:
            raise RuntimeError("Expected one Arduino CLI binary")
        with source.extractfile(members[0]) as data, CLI.open("wb") as output:
            shutil.copyfileobj(data, output)
    CLI.chmod(0o755)
    require_cli()


def install_library(library):
    archive = download(library["url"], library["sha256"])
    destination = LIBRARIES / library["name"]
    LIBRARIES.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=BUILD) as scratch:
        scratch = Path(scratch)
        with zipfile.ZipFile(archive) as source:
            for member in source.infolist():
                path = scratch / member.filename
                if not path.resolve().is_relative_to(scratch.resolve()):
                    raise RuntimeError("Invalid archive path")
            source.extractall(scratch)
        roots = list(scratch.glob("*/library.properties"))
        if len(roots) != 1:
            raise RuntimeError("Expected one library root")
        staged = roots[0].parent
        if library["name"] == "Arduino_TensorFlowLite":
            bridge = staged / "src/tensorflow/lite/micro"
            for header in (ROOT / "build-support/tflm-compat").glob("*.h"):
                shutil.copyfile(header, bridge / header.name)
            # The bundled WM8960/I2S example helpers own Nordic IRQs and use
            # Mbed PinName. KWS uses the core PDM driver, never these peripherals.
            for source in [
                *(staged / "src/peripherals").glob("*.cpp"),
                staged / "src/test_over_serial/test_over_serial.cpp",
            ]:
                source.write_text(
                    "#ifndef ARDUINO_ARCH_ZEPHYR\n" + source.read_text() + "\n#endif\n"
                )
            header = staged / "src/TensorFlowLite.h"
            header.write_text(
                header.read_text().replace(
                    '#include "peripherals/peripherals.h"',
                    '#ifndef ARDUINO_ARCH_ZEPHYR\n#include "peripherals/peripherals.h"\n#endif',
                )
            )
        manifest = tree_manifest(staged)
        # Preserve mtimes on an unchanged setup so incremental builds stay fast.
        if not destination.exists() or tree_manifest(destination) != manifest:
            if destination.exists():
                shutil.rmtree(destination)
            shutil.copytree(staged, destination)
    (destination / ".kws-manifest.json").write_text(
        json.dumps({"source": library, "files": manifest}, sort_keys=True)
    )


def tree_manifest(directory):
    return {
        str(path.relative_to(directory)): sha256(path)
        for path in sorted(directory.rglob("*"))
        if path.is_file() and path.name not in (".kws-manifest.json", ".DS_Store")
    }


def toolchain_asset():
    key = platform.system() + "-" + platform.machine().lower()
    asset = LOCK["toolchain"]["archives"].get(key)
    if not asset:
        raise RuntimeError("Unsupported compiler host: " + key)
    return asset


def toolchain_directory():
    if ZEPHYR:
        tool = LOCK["toolchain"]
        return (
            BUILD
            / "arduino-data/packages"
            / tool["packager"]
            / "tools"
            / tool["name"]
            / tool["version"]
        )
    return BUILD / "toolchain" / toolchain_asset()["file"].removesuffix(".tar.xz")


def verify_toolchain():
    if ZEPHYR:
        compiler = toolchain_directory() / "bin/arm-zephyr-eabi-g++"
        if not compiler.is_file():
            raise RuntimeError("Official Zephyr toolchain missing; run make setup")
        version = subprocess.check_output([str(compiler), "-dumpfullversion"], text=True).strip()
        if version != LOCK["toolchain"]["gcc_version"]:
            raise RuntimeError("Official Zephyr compiler version differs from lock")
        return
    directory = toolchain_directory()
    stamp = directory / ".kws-manifest.json"
    if not stamp.exists():
        raise RuntimeError("Pinned C++20 compiler missing; run make setup")
    manifest = json.loads(stamp.read_text())
    if manifest["source"] != LOCK["toolchain"] or tree_manifest(directory) != manifest["files"]:
        raise RuntimeError("Compiler files changed; run make setup")
    version = subprocess.check_output(
        [str(directory / "bin/arm-none-eabi-g++"), "-dumpfullversion"], text=True
    ).strip()
    if version != LOCK["toolchain"]["gcc_version"]:
        raise RuntimeError("Compiler version differs from dependencies.lock.json")


def install_toolchain():
    destination = toolchain_directory()
    if (destination / ".kws-manifest.json").exists():
        try:
            verify_toolchain()
            return
        except RuntimeError:
            pass
    asset = toolchain_asset()
    archive = download(LOCK["toolchain"]["base_url"] + asset["file"], asset["sha256"])
    with tempfile.TemporaryDirectory(dir=BUILD) as scratch:
        scratch = Path(scratch)
        with tarfile.open(archive) as source:
            for member in source.getmembers():
                path = scratch / member.name
                if not path.resolve().is_relative_to(scratch.resolve()):
                    raise RuntimeError("Invalid compiler archive path")
                if member.issym() or member.islnk():
                    target = (path.parent if member.issym() else scratch) / member.linkname
                    if not target.resolve().is_relative_to(scratch.resolve()):
                        raise RuntimeError("Invalid compiler archive link")
            source.extractall(scratch)
        staged = scratch / destination.name
        if not (staged / "bin/arm-none-eabi-g++").is_file():
            raise RuntimeError("Compiler archive has no expected toolchain root")
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists():
            shutil.rmtree(destination)
        shutil.move(str(staged), destination)
    (destination / ".kws-manifest.json").write_text(
        json.dumps(
            {"source": LOCK["toolchain"], "files": tree_manifest(destination)}, sort_keys=True
        )
    )
    verify_toolchain()


def verify_libraries():
    for library in LOCK["libraries"]:
        directory = LIBRARIES / library["name"]
        stamp = directory / ".kws-manifest.json"
        expected = {"source": library, "files": tree_manifest(directory)}
        if not stamp.exists() or json.loads(stamp.read_text()) != expected:
            raise RuntimeError(
                "Missing or modified dependency: " + library["name"] + "; run make setup"
            )
        if library["name"] == "Arduino_TensorFlowLite":
            for header in (ROOT / "build-support/tflm-compat").glob("*.h"):
                if sha256(header) != sha256(directory / "src/tensorflow/lite/micro" / header.name):
                    raise RuntimeError("Compatibility headers changed; run make setup")


def core_directory():
    return (
        BUILD
        / "arduino-data/packages/arduino/hardware"
        / LOCK["board"]["platform"].split(":")[1]
        / LOCK["board"]["version"]
    )


def restore_core_archive():
    # Verify/extract the complete official archive. Some CLI installs omit headers.
    board = LOCK["board"]
    archive = download(board["url"], board["sha256"])
    destination = core_directory()
    manifest = {}
    with tarfile.open(archive) as source:
        for member in source.getmembers():
            if not member.isfile():
                continue
            relative = Path(member.name).relative_to(
                "ArduinoCore-zephyr" if ZEPHYR else "ArduinoCore-mbed"
            )
            target = destination / relative
            if not target.resolve().is_relative_to(destination.resolve()):
                raise RuntimeError("Invalid core archive path")
            with source.extractfile(member) as content:
                data = content.read()
            expected = hashlib.sha256(data).hexdigest()
            manifest[str(relative)] = expected
            if not target.exists() or sha256(target) != expected:
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
                target.chmod(member.mode & 0o777)
                os.utime(target, (member.mtime, member.mtime))
    (BUILD / ("zephyr-core-manifest.json" if ZEPHYR else "core-manifest.json")).write_text(
        json.dumps({"source": board, "files": manifest}, sort_keys=True)
    )


def verify_core():
    cores = json.loads(cli("core", "list", "--json", capture=True).stdout).get("platforms", [])
    board = LOCK["board"]
    if not any(
        core["id"] == board["platform"] and core["installed_version"] == board["version"]
        for core in cores
    ):
        raise RuntimeError("Pinned board core missing; run make setup")
    stamp = BUILD / ("zephyr-core-manifest.json" if ZEPHYR else "core-manifest.json")
    if not stamp.exists():
        raise RuntimeError("Core source manifest missing; run make setup")
    manifest = json.loads(stamp.read_text())
    if manifest["source"] != board or any(
        not (core_directory() / name).exists() or sha256(core_directory() / name) != digest
        for name, digest in manifest["files"].items()
    ):
        raise RuntimeError("Core sources changed; run make setup")


def setup():
    install_cli()
    board = LOCK["board"]
    cli("core", "update-index")
    cli("core", "install", board["platform"] + "@" + board["version"])
    restore_core_archive()
    for library in LOCK["libraries"]:
        install_library(library)
    if not ZEPHYR:
        install_toolchain()
    verify_core()
    verify_libraries()


def compile_sketch(jobs, output):
    # Last -std flag wins over each core's default language standard.
    # Preinclude chrono before Arduino's abs macro, including in upstream C++ sources.
    cpp_flags = "-std=" + CPP_STANDARD + ' -include "' + str(SKETCH / "cpp_standard.h") + '"'
    if ZEPHYR:
        cpp_flags += " -DPDM_NUMBER_OF_SAMPLES=256 -DPDM_THREAD_PRIORITY=3"
    compiler_properties = (
        []
        if ZEPHYR
        else ["--build-property", "compiler.path=" + str(toolchain_directory() / "bin") + "/"]
    )
    cli(
        "compile",
        "--fqbn",
        LOCK["board"]["fqbn"],
        "--jobs",
        str(jobs),
        "--warnings",
        "default",
        "--build-property",
        "compiler.cpp.extra_flags=" + cpp_flags,
        *compiler_properties,
        "--build-path",
        BUILD / ("zephyr-objects" if ZEPHYR else "objects"),
        "--output-dir",
        output,
        SKETCH,
    )


def build(jobs):
    require_cli()
    verify_core()
    verify_libraries()
    verify_toolchain()
    SKETCH.mkdir(parents=True, exist_ok=True)
    expected = {
        path.name for path in (ROOT / "firmware").iterdir() if path.suffix in (".ino", ".cpp", ".h")
    }
    for path in SKETCH.iterdir():
        if path.is_file() and path.name not in expected:
            path.unlink()
    for name in expected:
        source = ROOT / "firmware" / name
        staged = SKETCH / name
        # A forced-include header must keep its timestamp when unchanged, or every
        # dependency C++ object would be rebuilt on each normal/stress build.
        if not staged.exists() or sha256(source) != sha256(staged):
            shutil.copyfile(source, staged)
    # Hash staged inputs before compiling, not live source files after a long build.
    provenance = {
        "dependencies": LOCK,
        "core": CORE,
        "build_tooling_sha256": sha256(Path(__file__)),
        "firmware": tree_manifest(SKETCH),
        "compatibility_headers": tree_manifest(ROOT / "build-support/tflm-compat"),
        "host": platform.platform(),
        "cpp_standard": CPP_STANDARD,
    }
    compile_sketch(jobs, OUTPUT)
    # Capture the exact inputs beside the ELF/BIN/HEX for review and CI artifacts.
    (OUTPUT / "build-manifest.json").write_text(json.dumps(provenance, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("setup", "build", "flash", "boards"))
    parser.add_argument("--port")
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    configure()
    if args.command == "setup":
        setup()
    elif args.command == "boards":
        require_cli()
        cli("board", "list")
    else:
        if args.command == "flash" and not args.port:
            parser.error("flash requires --port (make flash PORT=...)")
        build(args.jobs)
        if args.command == "flash":
            cli(
                "upload",
                "--fqbn",
                LOCK["board"]["fqbn"],
                "--port",
                args.port,
                "--input-dir",
                OUTPUT,
            )


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
