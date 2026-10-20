"""Checks build isolation and dependency integrity without network or a board."""

import hashlib
import io
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


if __name__ == "__main__":
    unittest.main()
