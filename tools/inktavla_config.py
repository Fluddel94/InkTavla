#!/usr/bin/env python3
"""Puts the configuration file onto the device over USB, or reads it back.

The device must be connected by USB and running the InkTavla firmware;
close the serial monitor first (only one program can use the port).

    python tools/inktavla_config.py put config.json --port COM5
    python tools/inktavla_config.py get backup.json --port COM5
    python tools/inktavla_config.py info --port /dev/ttyACM0
    python tools/inktavla_config.py log inktavla-log.txt --port COM5
    python tools/inktavla_config.py calendar-url --port COM5
    python tools/inktavla_config.py calendar-url --clear --port COM5

"put" checks the file, sends it and shows the problems the device found in
it (e.g. an invalid station ID). The device saves the file only if it is
valid JSON and then restarts with it.

"log" reads the system log of the device (oldest entry first) and writes it
to the file, or shows it without a file. Like on the settings page it holds
no addresses, Wi-Fi names or calendar titles.

"calendar-url" asks for the private iCal address of the calendar (the
input is not shown) and stores it in the device's protected storage, not in
the configuration file; it can be replaced or cleared, but never read back.

Requires pyserial (pip install -r tools/requirements.txt). The protocol is
described in lib/core/src/core/config_command.h.
"""

import argparse
import getpass
import json
import re
import sys
import time
import zlib

PREFIX = b"INKTAVLA "
MAX_BYTES = 16384
MAX_SECRET_BYTES = 1024
TIMEOUT_S = 10.0


def open_port(port, baud):
    import serial  # pyserial

    return serial.Serial(port, baud, timeout=0.2)


def read_reply_line(link, deadline):
    """Next line starting with "INKTAVLA " (without prefix), None on timeout.

    Other output of the device (log lines, Improv packets) is skipped.
    """
    buffer = b""
    while time.monotonic() < deadline:
        chunk = link.read(1)
        if not chunk:
            continue
        if chunk != b"\n":
            buffer += chunk
            if len(buffer) > 4096:
                buffer = b""
            continue
        line = buffer.rstrip(b"\r")
        buffer = b""
        if line.startswith(PREFIX):
            return line[len(PREFIX):].decode("utf-8", errors="replace")
    return None


def read_exact(link, count, deadline):
    data = b""
    while len(data) < count and time.monotonic() < deadline:
        data += link.read(count - len(data))
    return data


def command_put(link, path):
    with open(path, "rb") as f:
        data = f.read()
    if not data or len(data) > MAX_BYTES:
        print(f"{path}: must be 1 to {MAX_BYTES} bytes")
        return 1
    try:
        if not isinstance(json.loads(data.decode("utf-8")), dict):
            print(f"{path}: must contain a JSON object")
            return 1
    except (UnicodeDecodeError, ValueError) as error:
        print(f"{path}: not valid JSON ({error})")
        return 1

    header = f"CONFIG PUT {len(data)} {zlib.crc32(data):08x}\n".encode()
    link.write(b"\n" + PREFIX + header + data)
    link.flush()
    deadline = time.monotonic() + TIMEOUT_S
    while True:
        line = read_reply_line(link, deadline)
        if line is None:
            print("No answer from the device (is the InkTavla firmware running?)")
            return 1
        if line.startswith("ISSUE "):
            print("Issue: " + line[6:])
        elif line.startswith("OK"):
            print(line[2:].strip().capitalize() or "OK")
            return 0
        elif line.startswith("ERR"):
            print("Error: " + line[3:].strip())
            return 1


def command_get(link, path):
    link.write(b"\n" + PREFIX + b"CONFIG GET\n")
    link.flush()
    deadline = time.monotonic() + TIMEOUT_S
    while True:
        line = read_reply_line(link, deadline)
        if line is None:
            print("No answer from the device (is the InkTavla firmware running?)")
            return 1
        if line.startswith("ERR"):
            print("Error: " + line[3:].strip())
            return 1
        if line.startswith("DATA "):
            try:
                size_text, crc_text = line[5:].split()
                size, crc = int(size_text), int(crc_text, 16)
            except ValueError:
                print("Unexpected answer: " + line)
                return 1
            data = read_exact(link, size, deadline)
            if len(data) != size or zlib.crc32(data) != crc:
                print("Transfer incomplete or damaged, please try again")
                return 1
            if path:
                with open(path, "wb") as f:
                    f.write(data)
                print(f"Saved to {path} ({size} bytes)")
            else:
                sys.stdout.write(data.decode("utf-8", errors="replace") + "\n")
            return 0


def wait_ok(link):
    """Waits for "OK" or "ERR"; prints the answer, returns the exit code."""
    deadline = time.monotonic() + TIMEOUT_S
    while True:
        line = read_reply_line(link, deadline)
        if line is None:
            print("No answer from the device (is the InkTavla firmware running?)")
            return 1
        if line.startswith("OK"):
            print(line[2:].strip() or "OK")
            return 0
        if line.startswith("ERR"):
            print("Error: " + line[3:].strip())
            return 1


def read_calendar_url():
    """Asks for the address without showing it; None if invalid."""
    print("Private iCal address of the calendar (input is hidden).")
    print("Google Calendar: the calendar's 'Settings and sharing' -> 'Integrate calendar'")
    print("-> 'Secret address in iCal format'.")
    value = getpass.getpass("Address: ").strip()
    lower = value.lower()
    if not (lower.startswith("https://") or lower.startswith("webcal://")):
        print("The address must start with https:// or webcal://")
        return None
    if any(c <= " " or c > "~" for c in value) or len(value) > MAX_SECRET_BYTES:
        print("The address must be printable ASCII without spaces, "
              f"at most {MAX_SECRET_BYTES} characters")
        return None
    return value


def command_calendar_url(link, clear, value):
    if clear:
        link.write(b"\n" + PREFIX + b"SECRET CLEAR calendar_url\n")
    else:
        data = value.encode("ascii")
        header = f"SECRET SET calendar_url {len(data)} {zlib.crc32(data):08x}\n".encode()
        link.write(b"\n" + PREFIX + header + data)
    link.flush()
    return wait_ok(link)


def command_info(link):
    link.write(b"\n" + PREFIX + b"INFO\n")
    link.flush()
    deadline = time.monotonic() + TIMEOUT_S
    while True:
        line = read_reply_line(link, deadline)
        if line is None:
            print("No answer from the device (is the InkTavla firmware running?)")
            return 1
        if line.startswith("INFO "):
            print(line[5:])
            return 0
        if line.startswith("ERR"):
            print("Error: " + line[3:].strip())
            return 1


# A line of the log (core::formatLogLine()); other output of the device
# that arrives meanwhile (new entries echoed without time) is skipped.
LOG_LINE = re.compile(r"^(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d|\+[0-9d :]+) +(INFO|WARN|ERROR) ")


def command_log(link, path):
    link.write(b"\n" + PREFIX + b"LOG\n")
    link.flush()
    deadline = time.monotonic() + TIMEOUT_S
    line = read_reply_line(link, deadline)
    while line is not None and not line.startswith("LOG ") and not line.startswith("ERR"):
        line = read_reply_line(link, deadline)
    if line is None:
        print("No answer from the device (is the InkTavla firmware running?)")
        return 1
    if line.startswith("ERR"):
        print("Error: " + line[3:].strip() + " (firmware before 0.3.0 has no log)")
        return 1
    lines = []
    buffer = b""
    deadline = time.monotonic() + TIMEOUT_S
    while time.monotonic() < deadline:
        chunk = link.read(256)
        if not chunk:
            continue
        deadline = time.monotonic() + TIMEOUT_S  # still arriving
        buffer += chunk
        *complete, buffer = buffer.split(b"\n")
        for raw in complete:
            text = raw.rstrip(b"\r").decode("utf-8", errors="replace")
            if text == "INKTAVLA OK":
                return write_log(lines, path)
            # The header ("# ...", firmware 0.3.0 and later) and the entries.
            if text.startswith("# ") or LOG_LINE.match(text):
                lines.append(text)
    print("The log was not complete (no end mark), nothing saved.")
    return 1


def write_log(lines, path):
    text = "".join(line + "\n" for line in lines)
    entries = sum(1 for line in lines if not line.startswith("# "))
    if path:
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print(f"{entries} entries written to {path}")
    else:
        sys.stdout.write(text)
    return 0


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("command", choices=["put", "get", "info", "log", "calendar-url"])
    parser.add_argument("file", nargs="?",
                        help="configuration file (put: required), or the file for the log")
    parser.add_argument("--clear", action="store_true",
                        help="calendar-url: remove the stored address")
    parser.add_argument("--port", required=True, help="serial port, e.g. COM5 or /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args(argv)
    if args.command == "put" and not args.file:
        parser.error("put needs the configuration file")

    # Ask before opening the port, so that the device is not kept waiting.
    url = None
    if args.command == "calendar-url" and not args.clear:
        url = read_calendar_url()
        if url is None:
            return 1

    link = open_port(args.port, args.baud)
    try:
        if args.command == "calendar-url":
            return command_calendar_url(link, args.clear, url)
        if args.command == "put":
            return command_put(link, args.file)
        if args.command == "get":
            return command_get(link, args.file)
        if args.command == "log":
            return command_log(link, args.file)
        return command_info(link)
    finally:
        link.close()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
