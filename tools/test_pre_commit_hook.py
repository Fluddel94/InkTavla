"""Tests for .githooks/pre-commit: makes real commits in temporary
repositories. Needs git and sh (skipped without them, e.g. on Windows
outside Git Bash)."""

import os
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
HOOK = ROOT / ".githooks" / "pre-commit"
STRIP = ROOT / "tools" / "png_strip.py"
GIT = shutil.which("git")
SH = shutil.which("sh")


def chunk(kind, data):
    return (struct.pack(">I", len(data)) + kind + data
            + struct.pack(">I", zlib.crc32(kind + data)))


def png(metadata=b""):
    """A 1x1 gray PNG, with a tEXt chunk if metadata is given."""
    out = b"\x89PNG\r\n\x1a\n" + chunk(
        b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 0, 0, 0, 0))
    if metadata:
        out += chunk(b"tEXt", b"Author\0" + metadata)
    return out + chunk(b"IDAT", zlib.compress(b"\0\x80")) + chunk(b"IEND", b"")


@unittest.skipUnless(GIT and SH, "needs git and sh")
class PreCommitHookTest(unittest.TestCase):
    def setUp(self):
        self.repo = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.repo)
        self.env = dict(os.environ, HOME=str(self.repo / "home"),
                        GIT_CONFIG_NOSYSTEM="1")
        self.env.pop("USERPROFILE", None)
        (self.repo / "home").mkdir()
        self.git("init", "-q", "-b", "main")
        self.git("config", "user.name", "Test")
        self.git("config", "user.email", "test@example.invalid")
        self.git("config", "core.hooksPath", ".githooks")
        (self.repo / ".githooks").mkdir()
        shutil.copy(HOOK, self.repo / ".githooks" / "pre-commit")
        os.chmod(self.repo / ".githooks" / "pre-commit", 0o755)
        (self.repo / "tools").mkdir()
        shutil.copy(STRIP, self.repo / "tools" / "png_strip.py")

    def git(self, *arguments, check=True):
        return subprocess.run([GIT, *arguments], cwd=self.repo, env=self.env,
                              capture_output=True, text=True, check=check)

    def committed(self, path):
        return subprocess.run([GIT, "show", f"HEAD:{path}"], cwd=self.repo,
                              env=self.env, capture_output=True,
                              check=True).stdout

    def write(self, path, data):
        (self.repo / path).parent.mkdir(parents=True, exist_ok=True)
        (self.repo / path).write_bytes(data)

    def test_strips_staged_png_before_commit(self):
        self.write("images/a b.png", png(b"Jane Doe"))
        self.git("add", ".")
        result = self.git("commit", "-q", "-m", "add", check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.committed("images/a b.png"), png())
        self.assertEqual((self.repo / "images/a b.png").read_bytes(), png())
        self.assertEqual(self.git("status", "--porcelain").stdout, "")

    def test_only_staged_files_are_touched(self):
        self.write("staged.png", png(b"x"))
        self.write("other.png", png(b"y"))
        self.git("add", "staged.png", "tools", ".githooks")
        self.assertEqual(self.git("commit", "-q", "-m", "add",
                                  check=False).returncode, 0)
        self.assertEqual(self.committed("staged.png"), png())
        self.assertEqual((self.repo / "other.png").read_bytes(), png(b"y"))

    def test_replaces_personal_stl_header(self):
        self.write("case/a.stl", b"solid C:\\Users\\jane\\a\nendsolid\n")
        self.git("add", ".")
        self.assertEqual(self.git("commit", "-q", "-m", "add",
                                  check=False).returncode, 0)
        self.assertNotIn(b"jane", self.committed("case/a.stl"))

    def test_partly_staged_file_stops_the_commit(self):
        self.write("a.png", png(b"x"))
        self.git("add", ".")
        self.write("a.png", png(b"later"))
        result = self.git("commit", "-q", "-m", "add", check=False)
        self.assertEqual(result.returncode, 1)
        self.assertIn("changed again after staging", result.stderr)
        self.assertIn("a.png", result.stderr)
        self.assertEqual((self.repo / "a.png").read_bytes(), png(b"later"))
        self.assertNotEqual(self.git("rev-parse", "-q", "--verify", "HEAD",
                                     check=False).returncode, 0)

    def test_other_image_format_stops_the_commit(self):
        self.write("photo.JPG", b"\xff\xd8\xff\xe0 not really a JPEG")
        self.git("add", ".")
        result = self.git("commit", "-q", "-m", "add", check=False)
        self.assertEqual(result.returncode, 1)
        self.assertIn("JPG image", result.stdout + result.stderr)

    def test_without_python_the_commit_stops(self):
        bin_dir = self.repo / "home" / "bin"
        bin_dir.mkdir()
        os.symlink(GIT, bin_dir / "git")
        os.symlink(SH, bin_dir / "sh")
        self.write("a.png", png(b"x"))
        self.git("add", ".")
        self.env["PATH"] = str(bin_dir)
        result = self.git("commit", "-q", "-m", "add", check=False)
        self.assertEqual(result.returncode, 1)
        self.assertIn("no Python 3 found", result.stderr)

    def test_prefers_platformio_python(self):
        scripts = self.repo / "home" / ".platformio" / "penv" / "bin"
        scripts.mkdir(parents=True)
        marker = self.repo / "home" / "used"
        (scripts / "python").write_text(
            f'#!/bin/sh\ntouch "{marker}"\nexec "{sys.executable}" "$@"\n')
        os.chmod(scripts / "python", 0o755)
        self.write("a.png", png(b"x"))
        self.git("add", ".")
        self.assertEqual(self.git("commit", "-q", "-m", "add",
                                  check=False).returncode, 0)
        self.assertTrue(marker.exists())
        self.assertEqual(self.committed("a.png"), png())

    def test_userprofile_with_backslashes(self):
        # Windows: USERPROFILE=C:\Users\name, Python in penv\Scripts.
        profile = self.repo / "profile"
        scripts = profile / ".platformio" / "penv" / "Scripts"
        scripts.mkdir(parents=True)
        marker = self.repo / "home" / "used"
        (scripts / "python.exe").write_text(
            f'#!/bin/sh\ntouch "{marker}"\nexec "{sys.executable}" "$@"\n')
        os.chmod(scripts / "python.exe", 0o755)
        self.env["USERPROFILE"] = str(profile).replace("/", "\\")
        self.write("a.png", png(b"x"))
        self.git("add", ".")
        self.assertEqual(self.git("commit", "-q", "-m", "add",
                                  check=False).returncode, 0)
        self.assertTrue(marker.exists())

    def test_commit_without_images_needs_no_python(self):
        bin_dir = self.repo / "home" / "bin"
        bin_dir.mkdir()
        os.symlink(GIT, bin_dir / "git")
        os.symlink(SH, bin_dir / "sh")
        self.write("README.md", b"text\n")
        self.git("add", ".")
        self.env["PATH"] = str(bin_dir)
        self.assertEqual(self.git("commit", "-q", "-m", "add",
                                  check=False).returncode, 0)


if __name__ == "__main__":
    unittest.main()
