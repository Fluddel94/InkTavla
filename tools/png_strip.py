#!/usr/bin/env python3
"""Finds and removes personal details from the files of the repository.

Editors and other tools add details about the person who made or edited a
file (software, account, computer, time) to images and 3D models. Once the
repository is public these stay in its history for good, so files are
committed without them. Without an argument the whole repository is checked
(all folders except .git, .pio and build folders):

    python tools/png_strip.py           # check and clean the whole repository
    python tools/png_strip.py --check   # only check, change nothing (CI)
    python tools/png_strip.py test/fixtures/ui   # only these files or folders

Arguments are files or folders (searched including subfolders).

PNG: every metadata chunk is removed (content credentials caBX, text
tEXt/zTXt/iTXt, EXIF eXIf, time tIME and all other optional chunks). Only the
critical chunks (IHDR, PLTE, IDAT, IEND) and the transparency table tRNS are
kept, so the pixels stay exactly the same. There are no exceptions for
copyright notes: author and license are in README and LICENSE.

JPG, JPEG, WebP, GIF and other image formats: reported only, and --check fails.
Use PNG for images in the repository.

STL: the header (80 bytes of a binary STL, the name behind "solid" and
"endsolid" of an ASCII STL) may name the project, the license and the
repository, as the case generator writes it:

    InkTavla case - CC BY-NC-SA 4.0 - github.com/Fluddel94/InkTavla

The check fails only for details that nobody meant to publish: file paths
(C:\\, /home/, /Users/), user or computer names ("user: ...", "host=..."),
e-mail addresses. Cleaning replaces such a header by the intended one;
headers without these details stay as they are.

With --check nothing is written and the exit status is 1 if anything is
found (used by CI). Uses the Python standard library only.
"""

import os
import pathlib
import re
import struct
import sys

SIGNATURE = b"\x89PNG\r\n\x1a\n"
# Optional chunks that are needed to show the image as it is: the
# transparency table of palette, gray and RGB images without alpha channel.
KEEP_ANCILLARY = {b"tRNS"}

STL_NAME = "InkTavla case - CC BY-NC-SA 4.0 - github.com/Fluddel94/InkTavla"

OTHER_IMAGE_SUFFIXES = {
    ".jpg", ".jpeg", ".jpe", ".jfif", ".webp", ".gif",
    ".tif", ".tiff", ".heic", ".heif", ".avif",
}
SKIPPED_FOLDERS = {
    ".git", ".pio", "build", "dist", ".venv", "venv", "node_modules",
    "__pycache__",
}

UNINTENDED = [
    ("a file path", re.compile(r"[A-Za-z]:[\\/]|\\\\[\w.$-]+\\|[\\/]Users[\\/]"
                               r"|/home/|/root/")),
    ("an e-mail address", re.compile(r"[\w.+-]+@[\w-]+\.[\w.-]+")),
    ("a user or computer name",
     re.compile(r"\b(?:user(?:name)?|author|owner|host(?:name)?|computer|"
                r"machine|pc|benutzer|autor|rechner)\s*[:=]\s*\S+", re.I)),
]

ASCII_NAME_LINE = re.compile(rb"^([ \t]*(?:end)?solid)([^\r\n]*)", re.I | re.M)


def strip_png(data):
    """Returns the PNG without metadata chunks, or None if invalid."""
    if not data.startswith(SIGNATURE):
        return None
    out = bytearray(SIGNATURE)
    pos = len(SIGNATURE)
    while pos + 12 <= len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        end = pos + 12 + length
        if end > len(data):
            return None
        chunk_type = data[pos + 4:pos + 8]
        if chunk_type[0] & 0x20 == 0 or chunk_type in KEEP_ANCILLARY:
            out += data[pos:end]  # upper-case first letter: critical
        pos = end
        if chunk_type == b"IEND":
            return bytes(out)
    return None  # no IEND: cut off, do not write a broken image


def unintended(text):
    """Names the details in the text that nobody meant to publish."""
    return [name for name, pattern in UNINTENDED if pattern.search(text)]


def stl_kind(data):
    """'binary', 'ascii' or None if it does not look like an STL."""
    if len(data) >= 84:
        (count,) = struct.unpack("<I", data[80:84])
        if 84 + 50 * count == len(data):
            return "binary"
    if data.lstrip()[:5].lower() == b"solid":
        return "ascii"
    if len(data) >= 84 and 84 + 50 * count <= len(data):
        return "binary"
    return None


def check_stl(data):
    """Returns (found, cleaned): the unintended details in the header and the
    STL with the intended header instead (None if the STL is invalid or has
    nothing to clean)."""
    kind = stl_kind(data)
    if kind is None:
        return None, None
    if kind == "binary":
        # A NUL ends the text, but what follows is scanned too: old buffer
        # contents are where such details hide.
        text = data[:80].decode("latin-1").replace("\0", "\n")
        found = unintended(text)
        if not found:
            return [], None
        return found, STL_NAME.encode("ascii").ljust(80, b"\0") + data[80:]
    found = []
    for match in ASCII_NAME_LINE.finditer(data):
        found += [f for f in unintended(match.group(2).decode("latin-1"))
                  if f not in found]
    if not found:
        return [], None
    cleaned = ASCII_NAME_LINE.sub(
        lambda m: m.group(1) + b" " + STL_NAME.encode("ascii"), data)
    return found, cleaned


def files(arguments):
    """Yields (path, named): all files to look at; named is True for a file
    that was given as an argument and not found in a folder."""
    for argument in arguments:
        path = pathlib.Path(argument)
        if not path.is_dir():
            yield path, True
            continue
        for folder, subfolders, names in os.walk(path):
            subfolders[:] = sorted(s for s in subfolders
                                   if s not in SKIPPED_FOLDERS)
            for name in sorted(names):
                yield pathlib.Path(folder) / name, False


def main(arguments):
    if "-h" in arguments or "--help" in arguments:
        print(__doc__.strip())
        return 0
    check = "--check" in arguments
    arguments = [a for a in arguments if a != "--check"]
    if not arguments:
        arguments = [str(pathlib.Path(__file__).resolve().parent.parent)]
    status = 0
    cleaned_files = 0
    counts = {"PNG": 0, "STL": 0}
    for path, named in files(arguments):
        suffix = path.suffix.lower()
        if suffix == ".png":
            counts["PNG"] += 1
            data = path.read_bytes()
            cleaned = strip_png(data)
            if cleaned is None:
                print(f"{path}: not a valid PNG, skipped")
                status = 1
            elif cleaned != data and check:
                print(f"{path}: {len(data) - len(cleaned)} bytes of metadata, "
                      "run tools/png_strip.py on it")
                status = 1
            elif cleaned != data:
                path.write_bytes(cleaned)
                cleaned_files += 1
                print(f"{path}: removed {len(data) - len(cleaned)} bytes "
                      "of metadata")
        elif suffix == ".stl":
            counts["STL"] += 1
            data = path.read_bytes()
            found, cleaned = check_stl(data)
            if found is None:
                print(f"{path}: not a valid STL, skipped")
                status = 1
            elif found and check:
                print(f"{path}: header contains {', '.join(found)}, "
                      "run tools/png_strip.py on it")
                status = 1
            elif found:
                path.write_bytes(cleaned)
                cleaned_files += 1
                print(f"{path}: replaced the header (it contained "
                      f"{', '.join(found)})")
        elif suffix in OTHER_IMAGE_SUFFIXES:
            print(f"{path}: {suffix[1:].upper()} image may carry metadata "
                  "and cannot be cleaned here; use PNG or remove it")
            status = 1
        elif named:
            print(f"{path}: neither PNG nor STL, skipped")
            status = 1
    result = ("see above." if status else
              "cleaned." if cleaned_files else "nothing found.")
    print(f"Looked at {counts['PNG']} PNG and {counts['STL']} STL files: "
          + result)
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
