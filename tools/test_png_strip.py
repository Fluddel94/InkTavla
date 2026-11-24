#!/usr/bin/env python3
"""Tests for tools/png_strip.py (run: python -m unittest discover -s tools)."""

import contextlib
import io
import pathlib
import struct
import tempfile
import unittest
import zlib

import png_strip

NAME = png_strip.STL_NAME


def chunk(kind, payload=b""):
    body = kind + payload
    return (struct.pack(">I", len(payload)) + body
            + struct.pack(">I", zlib.crc32(body)))


def png(*extra, after_iend=b"", pixels=b"\x00\x01\x02\x03"):
    ihdr = struct.pack(">IIBBBBB", 1, 1, 8, 6, 0, 0, 0)
    chunks = [chunk(b"IHDR", ihdr)]
    chunks += list(extra)
    chunks += [chunk(b"IDAT", zlib.compress(b"\x00" + pixels)), chunk(b"IEND")]
    return png_strip.SIGNATURE + b"".join(chunks) + after_iend


METADATA = [
    chunk(b"caBX", b"c2pa content credentials"),
    chunk(b"tEXt", b"Author\x00Some Person"),
    chunk(b"zTXt", b"Comment\x00\x00" + zlib.compress(b"hello")),
    chunk(b"iTXt", b"XML:com.adobe.xmp\x00\x00\x00\x00\x00<x/>"),
    chunk(b"eXIf", b"MM\x00*"),
    chunk(b"tIME", b"\x07\xea\x0b\x18\x0c\x00\x00"),
    chunk(b"pHYs", b"\x00\x00\x0b\x13\x00\x00\x0b\x13\x01"),
]


def binary_stl(header, triangles=1):
    return header.ljust(80, b"\0") + struct.pack("<I", triangles) \
        + b"\0" * (50 * triangles)


def ascii_stl(name, end_name=None):
    end_name = name if end_name is None else end_name
    return (f"solid {name}\nfacet normal 0 0 1\nouter loop\n"
            "vertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\n"
            f"endsolid {end_name}\n").encode("latin-1")


def run(*arguments):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        status = png_strip.main([str(a) for a in arguments])
    return status, out.getvalue()


class PngTest(unittest.TestCase):
    def test_removes_every_metadata_chunk_and_keeps_the_image(self):
        for item in METADATA:
            data = png(item)
            cleaned = png_strip.strip_png(data)
            self.assertEqual(cleaned, png(), item[4:8])

    def test_pixels_stay_identical(self):
        data = png(*METADATA, pixels=b"\xff\x00\x10\x80")
        cleaned = png_strip.strip_png(data)
        self.assertEqual(cleaned, png(pixels=b"\xff\x00\x10\x80"))

    def test_keeps_transparency_table(self):
        data = png(METADATA[0], chunk(b"tRNS", b"\x00"), METADATA[1])
        self.assertEqual(png_strip.strip_png(data), png(chunk(b"tRNS", b"\x00")))

    def test_removes_data_after_iend(self):
        self.assertEqual(png_strip.strip_png(png(after_iend=b"secret")), png())

    def test_clean_png_is_unchanged_and_idempotent(self):
        data = png()
        self.assertEqual(png_strip.strip_png(data), data)

    def test_invalid_png(self):
        self.assertIsNone(png_strip.strip_png(b"not a png"))
        self.assertIsNone(png_strip.strip_png(png()[:-5]))


class StlTest(unittest.TestCase):
    def test_intended_header_is_allowed(self):
        self.assertEqual(png_strip.check_stl(binary_stl(NAME.encode())),
                         ([], None))
        self.assertEqual(png_strip.check_stl(ascii_stl(NAME)), ([], None))

    def test_name_fits_into_the_binary_header(self):
        self.assertLessEqual(len(NAME.encode("ascii")), 80)

    def test_harmless_headers_are_left_alone(self):
        for header in (b"", b"Exported from a CAD program", b"solid case"):
            self.assertEqual(png_strip.check_stl(binary_stl(header))[0], [])
        self.assertEqual(png_strip.check_stl(ascii_stl("case"))[0], [])

    def test_forbidden_binary_headers(self):
        cases = {
            b"C:\\Users\\Someone\\Documents\\case.stl": "a file path",
            b"D:/models/case.stl": "a file path",
            b"/home/someone/case.stl": "a file path",
            b"/Users/someone/case.stl": "a file path",
            b"made by someone@example.com": "an e-mail address",
            b"user: someone": "a user or computer name",
            b"Host=SOME-PC": "a user or computer name",
            # old buffer contents behind the end of the text
            NAME.encode() + b"\0C:\\Users\\x": "a file path",
        }
        for header, expected in cases.items():
            data = binary_stl(header, triangles=2)
            found, cleaned = png_strip.check_stl(data)
            self.assertIn(expected, found, header)
            self.assertEqual(cleaned, binary_stl(NAME.encode(), triangles=2))
            self.assertEqual(png_strip.check_stl(cleaned), ([], None))

    def test_forbidden_ascii_names(self):
        for name in ("C:\\Users\\Someone\\case.stl", "someone@example.com",
                     "/home/someone/case"):
            found, cleaned = png_strip.check_stl(ascii_stl("case", name))
            self.assertTrue(found, name)
            self.assertEqual(cleaned, ascii_stl(NAME))
            found, cleaned = png_strip.check_stl(ascii_stl(name))
            self.assertTrue(found, name)
            self.assertEqual(cleaned, ascii_stl(NAME))

    def test_triangles_stay_identical(self):
        data = binary_stl(b"C:\\x", triangles=3)
        data = data[:84] + bytes(range(150))
        _, cleaned = png_strip.check_stl(data)
        self.assertEqual(cleaned[80:], data[80:])

    def test_not_an_stl(self):
        self.assertEqual(png_strip.check_stl(b"hello"), (None, None))


class MainTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.temp.name)
        self.addCleanup(self.temp.cleanup)

    def write(self, relative, data):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def test_check_looks_into_all_folders_but_changes_nothing(self):
        dirty = png(*METADATA)
        top = self.write("a.png", dirty)
        deep = self.write("docs/img/b.png", dirty)
        status, out = run("--check", self.root)
        self.assertEqual(status, 1)
        self.assertIn("a.png", out)
        self.assertIn(str(pathlib.Path("docs/img/b.png")), out)
        self.assertEqual(top.read_bytes(), dirty)
        self.assertEqual(deep.read_bytes(), dirty)

    def test_cleaning_writes_png_and_stl(self):
        dirty = png(*METADATA)
        image = self.write("docs/b.png", dirty)
        model = self.write("case/case.stl", binary_stl(b"C:\\Users\\x"))
        status, _ = run(self.root)
        self.assertEqual(status, 0)
        self.assertEqual(image.read_bytes(), png())
        self.assertEqual(model.read_bytes(), binary_stl(NAME.encode()))
        self.assertEqual(run("--check", self.root)[0], 0)

    def test_stl_with_intended_header_passes_the_check(self):
        self.write("case/case.stl", binary_stl(NAME.encode()))
        self.write("case/ascii.stl", ascii_stl(NAME))
        self.assertEqual(run("--check", self.root)[0], 0)

    def test_stl_with_forbidden_header_fails_the_check(self):
        model = self.write("case/case.stl", binary_stl(b"/home/someone/x"))
        status, out = run("--check", self.root)
        self.assertEqual(status, 1)
        self.assertIn("a file path", out)
        self.assertEqual(model.read_bytes(), binary_stl(b"/home/someone/x"))

    def test_skips_git_pio_and_build_folders(self):
        dirty = png(*METADATA)
        for folder in (".git", ".pio", "build"):
            self.write(f"{folder}/x.png", dirty)
            self.write(f"{folder}/x.jpg", b"jpeg")
        status, out = run("--check", self.root)
        self.assertEqual(status, 0, out)

    def test_other_image_formats_are_reported(self):
        for name in ("a.jpg", "b.JPEG", "c.webp", "d.gif"):
            self.write(f"img/{name}", b"data")
        status, out = run("--check", self.root)
        self.assertEqual(status, 1)
        for name in ("a.jpg", "b.JPEG", "c.webp", "d.gif"):
            self.assertIn(name, out)
        self.assertEqual(run(self.root)[0], 1)

    def test_a_folder_given_as_argument_is_used_even_inside_pio(self):
        self.write(".pio/preview/x.png", png(*METADATA))
        status, _ = run("--check", self.root / ".pio" / "preview")
        self.assertEqual(status, 1)

    def test_file_argument_and_invalid_files(self):
        good = self.write("good.png", png())
        bad = self.write("bad.png", b"nope")
        self.assertEqual(run("--check", good)[0], 0)
        self.assertEqual(run("--check", bad)[0], 1)
        self.assertEqual(run("--check", self.write("x.txt", b"t"))[0], 1)


if __name__ == "__main__":
    unittest.main()
