#!/usr/bin/env python3
"""Warns about per-commit AI notes and links to AI chat sessions.

The README states once, under "Contributing", that InkTavla is developed with
the help of Claude. Commits, pull requests and files therefore need no notes
of their own (co-author lines, "generated with" footers), and links to chat
sessions should never be added: they can lead to private conversations, and
once the repository is public they stay in the history for good (pull
requests cannot be deleted at all). For contributors this is a request, not a
requirement, so this check only warns.

    python tools/check_ai_notes.py              # all files tracked by git
    git log -1 --format=%B | python tools/check_ai_notes.py --stdin "commit"

Options: --stdin LABEL checks text from standard input (shown as LABEL)
instead of the files, --strict exits with status 1 if something was found. In GitHub Actions the
findings appear as warnings. Uses the Python standard library only.
"""

import os
import pathlib
import re
import subprocess
import sys

PATTERNS = [
    re.compile(r"claude\.ai/code/session", re.IGNORECASE),
    re.compile(r"claude-session\s*:", re.IGNORECASE),
    re.compile(r"co-authored-by\s*:\s*claude", re.IGNORECASE),
    re.compile(r"generated with \[?claude", re.IGNORECASE),
    re.compile(r"noreply@anthropic\.com", re.IGNORECASE),
]

# This file names the patterns in its description and code.
SELF = pathlib.Path(__file__).resolve()


def tracked_files():
    try:
        out = subprocess.run(["git", "ls-files", "-z"], capture_output=True, check=True).stdout
        names = [n for n in out.decode("utf-8").split("\0") if n]
    except (OSError, subprocess.CalledProcessError):
        names = [
            str(p)
            for p in pathlib.Path(".").rglob("*")
            if p.is_file() and not {".git", ".pio"} & set(p.parts)
        ]
    return [pathlib.Path(n) for n in names]


def findings(label, text):
    for number, line in enumerate(text.splitlines(), start=1):
        if any(p.search(line) for p in PATTERNS):
            yield label, number


def report(label, number, is_file):
    message = "AI note or chat session link (see tools/check_ai_notes.py)"
    if os.environ.get("GITHUB_ACTIONS") == "true":
        where = f"file={label},line={number}" if is_file else ""
        print(f"::warning {where}::{message}" + ("" if is_file else f" in {label}, line {number}"))
    else:
        print(f"{label}:{number}: {message}")


def main(arguments):
    strict = "--strict" in arguments
    arguments = [a for a in arguments if a != "--strict"]
    count = 0
    for path in [] if "--stdin" in arguments else tracked_files():
        if path.resolve() == SELF or not path.is_file():
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue  # binary or unreadable
        for label, number in findings(path.as_posix(), text):
            report(label, number, True)
            count += 1
    if "--stdin" in arguments:
        index = arguments.index("--stdin")
        label = arguments[index + 1] if index + 1 < len(arguments) else "input"
        for found, number in findings(label, sys.stdin.read()):
            report(found, number, False)
            count += 1
    print(f"check_ai_notes: {count} finding(s)")
    return 1 if strict and count else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
