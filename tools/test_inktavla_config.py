#!/usr/bin/env python3
"""Tests for tools/inktavla_config.py without a device (run: python -m
unittest discover -s tools)."""

import contextlib
import io
import pathlib
import tempfile
import unittest
import zlib
from unittest import mock

import inktavla_config


class FakeLink:
    """Answers each CONFIG GET with the next prepared reply."""

    def __init__(self, replies):
        self.replies = list(replies)
        self.pending = b""
        self.requests = 0

    def write(self, data):
        if b"CONFIG GET" in data:
            self.requests += 1
            self.pending += self.replies.pop(0) if self.replies else b""

    def flush(self):
        pass

    def read(self, count=1):
        data, self.pending = self.pending[:count], self.pending[count:]
        return data

    def reset_input_buffer(self):
        self.pending = b""


def reply(data, sent=None):
    """DATA line with the checksum of data, followed by sent (default data)."""
    body = data if sent is None else sent
    return (b"INKTAVLA DATA %d %08x\n" % (len(data), zlib.crc32(data))
            + body + b"\nINKTAVLA OK\n")


CONFIG = b'{\n  "language": "en"\n}\n'
# A log line of the device in the middle of the transfer.
DAMAGED = reply(CONFIG, CONFIG[:5] + b"INFO  mvg  works\n" + CONFIG[5:])


def run_get(link, path=None):
    out = io.StringIO()
    with contextlib.redirect_stdout(out), mock.patch.object(inktavla_config.time, "sleep"):
        code = inktavla_config.command_get(link, path)
    return code, out.getvalue()


class GetTest(unittest.TestCase):
    def test_saves_the_file(self):
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder) / "backup.json"
            code, _ = run_get(FakeLink([reply(CONFIG)]), str(path))
            self.assertEqual(0, code)
            self.assertEqual(CONFIG, path.read_bytes())

    def test_asks_again_after_a_damaged_transfer(self):
        link = FakeLink([DAMAGED, reply(CONFIG)])
        code, out = run_get(link)
        self.assertEqual(0, code)
        self.assertEqual(2, link.requests)
        self.assertIn("asking again", out)
        self.assertIn('"language": "en"', out)

    def test_gives_up_after_three_damaged_transfers(self):
        link = FakeLink([DAMAGED] * 4)
        code, out = run_get(link)
        self.assertEqual(1, code)
        self.assertEqual(inktavla_config.GET_ATTEMPTS, link.requests)
        self.assertIn("please try again", out)

    def test_an_error_is_not_repeated(self):
        link = FakeLink([b"INKTAVLA ERR no configuration file\n"])
        code, out = run_get(link)
        self.assertEqual(1, code)
        self.assertEqual(1, link.requests)
        self.assertIn("no configuration file", out)


if __name__ == "__main__":
    unittest.main()
