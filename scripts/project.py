#!/usr/bin/env python3
"""Pinned, project-local Arduino build. Python standard library only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / ".build"
LOCK = json.loads((ROOT / "dependencies.lock.json").read_text())
CLI = BUILD / "bin" / "arduino-cli"
CONFIG = BUILD / "arduino-cli.json"
SKETCH = BUILD / "zephyr-sketch" / "kwskit"
LIBRARIES = BUILD / "arduino-user" / "libraries"
OUTPUT = BUILD / "zephyr-artifacts"
CPP_STANDARD = "gnu++20"
CPP_SAFETY_FLAGS = "-fno-exceptions -fno-rtti -Werror=vla"
PDM_READ_FAILURE = "\t\tif (ret < 0) {\n\t\t\tcontinue;\n\t\t}"
PDM_READ_BACKOFF = (
    "\t\tif (ret < 0) {\n"
    "\t\t\t// Yield on device failure so acquisition cannot starve application threads.\n"
    "\t\t\tk_msleep(1);\n\t\t\tcontinue;\n\t\t}"
)


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
            # KWS uses the core PDM driver. Exclude unrelated I2S example
            # helpers and the serial-test constructor from this dependency.
            # micro_time.cpp still includes the generic timer declarations.
            for source in (staged / "src/peripherals").iterdir():
                if source.name != "utility.h":
                    if source.is_dir():
                        shutil.rmtree(source)
                    else:
                        source.unlink()
            shutil.rmtree(staged / "src/test_over_serial")
            header = staged / "src/TensorFlowLite.h"
            header.write_text(
                header.read_text().replace('#include "peripherals/peripherals.h"', "")
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


def toolchain_directory():
    tool = LOCK["toolchain"]
    return (
        BUILD
        / "arduino-data/packages"
        / tool["packager"]
        / "tools"
        / tool["name"]
        / tool["version"]
    )


def verify_toolchain():
    compiler = toolchain_directory() / "bin/arm-zephyr-eabi-g++"
    if not compiler.is_file():
        raise RuntimeError("Official Zephyr toolchain missing; run make setup")
    version = subprocess.check_output([str(compiler), "-dumpfullversion"], text=True).strip()
    if version != LOCK["toolchain"]["gcc_version"]:
        raise RuntimeError("Official Zephyr compiler version differs from lock")


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
            relative = Path(member.name).relative_to("ArduinoCore-zephyr")
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
    (BUILD / "zephyr-core-manifest.json").write_text(
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
    stamp = BUILD / "zephyr-core-manifest.json"
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
    verify_toolchain()
    verify_core()
    verify_libraries()


def stage_pdm_library():
    """Stage a minimal driver override without modifying the verified official core."""
    source = core_directory() / "libraries/PDM"
    destination = BUILD / "pdm-realtime/PDM"
    inputs = [source / "library.properties", *sorted((source / "src").rglob("*"))]
    expected = set()
    for path in inputs:
        if not path.is_file():
            continue
        relative = path.relative_to(source)
        expected.add(relative)
        data = path.read_bytes()
        if relative == Path("src/PDM.cpp"):
            text = data.decode()
            if text.count(PDM_READ_FAILURE) != 1:
                raise RuntimeError("Pinned PDM read-failure context changed; review backoff patch")
            data = text.replace(PDM_READ_FAILURE, PDM_READ_BACKOFF).encode()
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists() or target.read_bytes() != data:
            target.write_bytes(data)
    if Path("src/PDM.cpp") not in expected:
        raise RuntimeError("Pinned core PDM source is missing")
    for path in destination.rglob("*"):
        if path.is_file() and path.relative_to(destination) not in expected:
            path.unlink()
    return destination


def compile_sketch(jobs, output):
    # Upload discovers images in this generated directory. Never mix sketch basenames.
    output.mkdir(parents=True, exist_ok=True)
    for artifact in output.glob("*.ino*"):
        if artifact.is_file() and not artifact.name.startswith(SKETCH.name + ".ino"):
            artifact.unlink()
    # Last -std flag wins over each core's default language standard.
    # Preinclude chrono before Arduino's abs macro, including in upstream C++ sources.
    cpp_flags = "-std=" + CPP_STANDARD + ' -include "' + str(SKETCH / "cpp_standard.h") + '"'
    cpp_flags += " -DPDM_NUMBER_OF_SAMPLES=256 -DPDM_THREAD_PRIORITY=3"
    cpp_flags += " " + CPP_SAFETY_FLAGS
    cli(
        "compile",
        "--fqbn",
        LOCK["board"]["fqbn"],
        "--library",
        stage_pdm_library(),
        "--jobs",
        str(jobs),
        "--warnings",
        "default",
        "--build-property",
        "compiler.cpp.extra_flags=" + cpp_flags,
        "--build-path",
        BUILD / "zephyr-objects",
        "--output-dir",
        output,
        SKETCH,
    )


def audit_realtime_objects(output):
    """Reject direct heap, exception and out-of-line atomic calls in sketch objects."""
    archive = BUILD / "zephyr-objects/sketch/objs.a"
    if not archive.is_file():
        raise RuntimeError("Missing sketch archive for real-time object audit")
    symbols = subprocess.check_output(
        [
            str(toolchain_directory() / "bin/arm-zephyr-eabi-nm"),
            "--undefined-only",
            "--demangle",
            "--print-file-name",
            str(archive),
        ],
        text=True,
    )
    imports = []
    forbidden = []
    heap_api = re.compile(
        r"^(?:(?:z_impl_)?k_(?:malloc|calloc|realloc|free|aligned_alloc|"
        r"object_alloc|object_free|thread_stack_alloc|thread_stack_free|"
        r"msgq_alloc_init|stack_alloc_init|pipe_alloc_init)|"
        r"(?:z_impl_)?(?:k|sys)_heap_(?:alloc|aligned_alloc|calloc|realloc|aligned_realloc|free)|"
        r"(?:__libc_)?(?:malloc|calloc|realloc|free)|_(?:malloc|calloc|realloc|free)_r|"
        r"reallocarray|aligned_alloc|posix_memalign|memalign|valloc|pvalloc|sbrk|_sbrk|alloca)$"
    )
    for line in symbols.splitlines():
        match = re.match(r"^(.+?):\s+[Uw]\s+(.+)$", line)
        if not match:
            continue
        owner, symbol = match.groups()
        entry = {"object": owner.rsplit(":", 1)[-1], "symbol": symbol}
        imports.append(entry)
        placement_new = symbol.startswith("operator new(") and symbol.endswith(", void*)")
        if (
            heap_api.fullmatch(symbol)
            or (symbol.startswith(("operator new", "operator delete")) and not placement_new)
            or symbol.startswith(("__atomic_", "__cxa_throw", "__cxa_allocate_exception"))
        ):
            forbidden.append(entry)
    if not imports:
        raise RuntimeError("No imports found in sketch archive; cannot verify real-time policy")
    if forbidden:
        raise RuntimeError("Forbidden real-time imports: " + json.dumps(forbidden))
    report = {
        "result": "passed",
        "scope": "Direct undefined references in all maintained sketch objects",
        "limits": "Not a transitive call-graph, loader heap or WCET proof",
        "cpp_safety_flags": CPP_SAFETY_FLAGS,
        "imports": imports,
    }
    (output / "realtime-audit.json").write_text(json.dumps(report, indent=2) + "\n")
    print("PASS: sketch object allocation/exception/atomic audit", flush=True)


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
        "core": "zephyr",
        "build_tooling_sha256": sha256(Path(__file__)),
        "firmware": tree_manifest(SKETCH),
        "compatibility_headers": tree_manifest(ROOT / "build-support/tflm-compat"),
        "pdm_driver": tree_manifest(stage_pdm_library()),
        "host": platform.platform(),
        "cpp_standard": CPP_STANDARD,
        "cpp_safety_flags": CPP_SAFETY_FLAGS,
    }
    compile_sketch(jobs, OUTPUT)
    audit_realtime_objects(OUTPUT)
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
