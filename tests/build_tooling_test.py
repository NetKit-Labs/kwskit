"""Checks build isolation and dependency integrity without network or a board."""

import hashlib
import io
import json
import tarfile
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import zipfile

spec = importlib.util.spec_from_file_location(
    "project", Path(__file__).resolve().parents[1] / "scripts/project.py"
)
project = importlib.util.module_from_spec(spec)
with mock.patch.dict(os.environ, {"KWS_CORE": "mbed"}):
    spec.loader.exec_module(project)


class BuildToolingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="kws-tooling-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.patchers = [
            mock.patch.object(project, "BUILD", self.root),
            mock.patch.object(project, "LIBRARIES", self.root / "libraries"),
        ]
        for patcher in self.patchers:
            patcher.start()
            self.addCleanup(patcher.stop)

    def archive(self, name, contents):
        path = self.root / name
        with zipfile.ZipFile(path, "w") as archive:
            for member, content in contents.items():
                archive.writestr(member, content)
        return path

    def test_verified_cache_and_bad_checksum(self):
        payload = self.root / "payload"
        payload.write_bytes(b"pinned source")
        digest = hashlib.sha256(payload.read_bytes()).hexdigest()
        result = project.download(payload.as_uri(), digest)
        self.assertEqual(result.read_bytes(), payload.read_bytes())
        payload.unlink()
        self.assertEqual(project.download("https://unreachable.invalid", digest), result)
        with self.assertRaisesRegex(RuntimeError, "Checksum mismatch"):
            project.download(result.as_uri(), "0" * 64)

    def test_archive_cannot_escape_build_directory(self):
        archive = self.archive(
            "bad.zip", {"../escape": "bad", "lib/library.properties": "version=1"}
        )
        with mock.patch.object(project, "download", return_value=archive):
            with self.assertRaisesRegex(RuntimeError, "Invalid archive path"):
                project.install_library({"name": "test", "url": "unused", "sha256": "123"})
        self.assertFalse((self.root / "escape").exists())

    def test_library_changes_and_lock_changes_require_setup(self):
        archive = self.archive(
            "good.zip", {"lib/library.properties": "version=1", "lib/src/a.h": "original"}
        )
        library = {"name": "test", "url": "unused", "sha256": "123"}
        with (
            mock.patch.object(project, "download", return_value=archive),
            mock.patch.object(project, "LOCK", {"libraries": [library]}),
        ):
            project.install_library(library)
            project.verify_libraries()
            directory = project.LIBRARIES / "test"
            (directory / "src/a.h").write_text("modified")
            with self.assertRaisesRegex(RuntimeError, "Missing or modified"):
                project.verify_libraries()
            project.install_library(library)
            with mock.patch.object(project, "LOCK", {"libraries": [dict(library, sha256="456")]}):
                with self.assertRaisesRegex(RuntimeError, "Missing or modified"):
                    project.verify_libraries()

    def test_core_archive_restores_missing_headers_and_preserves_existing_files(self):
        archive = self.root / "core.tar.gz"
        with tarfile.open(archive, "w:gz") as output:
            info = tarfile.TarInfo("ArduinoCore-mbed/include/cmsis_os.h")
            data = b"official header"
            info.size = len(data)
            info.mtime = 1000
            output.addfile(info, io.BytesIO(data))
        with mock.patch.object(project, "download", return_value=archive):
            project.restore_core_archive()
            header = project.core_directory() / "include/cmsis_os.h"
            self.assertEqual(header.read_bytes(), b"official header")
            stamp = header.stat().st_mtime_ns
            project.restore_core_archive()
            self.assertEqual(header.stat().st_mtime_ns, stamp)
            header.write_text("modified")
            project.restore_core_archive()
            self.assertEqual(header.read_bytes(), b"official header")

    def test_cli_ignores_ambient_arduino_settings(self):
        with (
            mock.patch.dict(os.environ, {"ARDUINO_DIRECTORIES_USER": "/foreign/libraries"}),
            mock.patch.object(project.subprocess, "run") as child,
        ):
            project.cli("version")
            kwargs = child.call_args.kwargs
            self.assertNotIn("ARDUINO_DIRECTORIES_USER", kwargs["env"])
            command = child.call_args.args[0]
            self.assertIn("--config-file", command)
            self.assertIn(str(project.CONFIG), command)

    def test_normal_and_stress_compiler_require_cpp20(self):
        with mock.patch.object(project, "cli") as cli:
            for output in (self.root / "artifacts", self.root / "stress-artifacts"):
                project.compile_sketch(4, output)
                args = cli.call_args.args
                self.assertEqual(args[0], "compile")
                self.assertEqual(
                    args[args.index("--build-property") + 1],
                    'compiler.cpp.extra_flags=-std=gnu++20 -include "'
                    + str(project.SKETCH / "cpp_standard.h")
                    + '"',
                )
                self.assertEqual(args[args.index("--output-dir") + 1], output)
                self.assertIn(
                    "compiler.path=" + str(project.toolchain_directory() / "bin") + "/", args
                )

    def test_zephyr_uses_official_compiler_and_pdm_configuration(self):
        with mock.patch.object(project, "ZEPHYR", True), mock.patch.object(project, "cli") as cli:
            project.compile_sketch(2, self.root / "zephyr-artifacts")
            args = cli.call_args.args
            flags = args[args.index("--build-property") + 1]
            self.assertIn("-std=gnu++20", flags)
            self.assertIn("-DPDM_NUMBER_OF_SAMPLES=256", flags)
            self.assertIn("-DPDM_THREAD_PRIORITY=3", flags)
            self.assertFalse(any(str(arg).startswith("compiler.path=") for arg in args))
            self.assertIn(project.BUILD / "zephyr-objects", args)

    def test_build_manifest_records_language_and_staged_sources(self):
        sketch = self.root / "sketch"
        output = self.root / "artifacts"
        output.mkdir()
        with (
            mock.patch.object(project, "SKETCH", sketch),
            mock.patch.object(project, "OUTPUT", output),
            mock.patch.object(project, "require_cli"),
            mock.patch.object(project, "verify_core"),
            mock.patch.object(project, "verify_libraries"),
            mock.patch.object(project, "verify_toolchain"),
            mock.patch.object(project, "compile_sketch") as compile_sketch,
        ):
            project.build(2)
            compile_sketch.assert_called_once_with(2, output)
            manifest = json.loads((output / "build-manifest.json").read_text())
            self.assertEqual(manifest["cpp_standard"], "gnu++20")
            self.assertEqual(manifest["firmware"], project.tree_manifest(sketch))
            self.assertIn("cpp_standard.h", manifest["firmware"])
            header = sketch / "cpp_standard.h"
            stamp = header.stat().st_mtime_ns
            project.build(2)
            self.assertEqual(header.stat().st_mtime_ns, stamp)
            header.write_text("outdated staged copy")
            project.build(2)
            self.assertEqual(
                header.read_bytes(), (project.ROOT / "firmware/cpp_standard.h").read_bytes()
            )

    def compiler_archive(self, entries):
        archive = self.root / "compiler.tar.xz"
        root = project.toolchain_directory().name
        with tarfile.open(archive, "w:xz") as output:
            for name, content, link in entries:
                info = tarfile.TarInfo(root + "/" + name)
                if link:
                    info.type = tarfile.SYMTYPE
                    info.linkname = content
                    output.addfile(info)
                else:
                    info.size = len(content)
                    info.mode = 0o755 if name.startswith("bin/") else 0o644
                    output.addfile(info, io.BytesIO(content))
        return archive

    def test_toolchain_integrity_version_and_restoration(self):
        archive = self.compiler_archive(
            [
                ("bin/arm-none-eabi-g++", b"compiler", False),
                ("include/example.h", b"original", False),
            ]
        )
        with (
            mock.patch.object(project, "download", return_value=archive),
            mock.patch.object(project.subprocess, "check_output", return_value="13.3.1\n"),
        ):
            project.install_toolchain()
            project.verify_toolchain()
            header = project.toolchain_directory() / "include/example.h"
            stamp = header.stat().st_mtime_ns
            project.install_toolchain()
            self.assertEqual(header.stat().st_mtime_ns, stamp)
            header.write_text("changed")
            with self.assertRaisesRegex(RuntimeError, "Compiler files changed"):
                project.verify_toolchain()
            project.install_toolchain()
            self.assertEqual(header.read_bytes(), b"original")
            with mock.patch.object(project.subprocess, "check_output", return_value="7.2.1\n"):
                with self.assertRaisesRegex(RuntimeError, "Compiler version differs"):
                    project.verify_toolchain()

    def test_toolchain_archive_rejects_path_and_link_escape(self):
        for entry in (("../../escape", b"bad", False), ("bin/escape", "../../../escape", True)):
            with self.subTest(entry=entry):
                archive = self.compiler_archive([entry])
                with mock.patch.object(project, "download", return_value=archive):
                    with self.assertRaisesRegex(RuntimeError, "Invalid compiler archive"):
                        project.install_toolchain()
                self.assertFalse((self.root / "escape").exists())


if __name__ == "__main__":
    unittest.main()
