#!/usr/bin/env python3
"""Removes metadata from PNG files, keeping only the image data.

Some tools add ancillary chunks to PNG files when they copy or open them
(content credentials, text, timestamps). The reference images of the screen
previews (test/fixtures/ui/) should contain nothing but the image, so run
this before committing them:

    python tools/png_strip.py test/fixtures/ui

Arguments are PNG files or folders (all *.png in them). Only critical chunks
(IHDR, PLTE, IDAT, IEND) are kept; the pixels do not change. Uses the Python
standard library only.
"""

import pathlib
import struct
import sys

SIGNATURE = b"\x89PNG\r\n\x1a\n"


def strip(data):
    """Returns the PNG with only its critical chunks, or None if invalid."""
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
        if chunk_type[0] & 0x20 == 0:  # upper-case first letter: critical
            out += data[pos:end]
        pos = end
        if chunk_type == b"IEND":
            break
    return bytes(out)


def files(arguments):
    for argument in arguments:
        path = pathlib.Path(argument)
        if path.is_dir():
            yield from sorted(path.glob("*.png"))
        else:
            yield path


def main(arguments):
    if not arguments:
        print(__doc__.strip())
        return 2
    status = 0
    for path in files(arguments):
        data = path.read_bytes()
        cleaned = strip(data)
        if cleaned is None:
            print(f"{path}: not a valid PNG, skipped")
            status = 1
        elif cleaned != data:
            path.write_bytes(cleaned)
            print(f"{path}: removed {len(data) - len(cleaned)} bytes of metadata")
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
