#!/usr/bin/env python3
"""Puts the configuration file onto the device over USB, or reads it back.

The device must be connected by USB and running the InkTavla firmware;
close the serial monitor first (only one program can use the port).

    python tools/inktavla_config.py put config.json --port COM5
    python tools/inktavla_config.py get backup.json --port COM5
    python tools/inktavla_config.py info --port /dev/ttyACM0

"put" checks the file, sends it and shows the problems the device found in
it (e.g. an invalid station ID). The device saves the file only if it is
valid JSON and then restarts with it. Requires pyserial
(pip install -r tools/requirements.txt). The protocol is described in
lib/core/src/core/config_command.h.
"""

import argparse
import json
import sys
import time
import zlib

PREFIX = b"INKTAVLA "
MAX_BYTES = 16384
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


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("command", choices=["put", "get", "info"])
    parser.add_argument("file", nargs="?", help="configuration file (put: required)")
    parser.add_argument("--port", required=True, help="serial port, e.g. COM5 or /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args(argv)
    if args.command == "put" and not args.file:
        parser.error("put needs the configuration file")

    link = open_port(args.port, args.baud)
    try:
        if args.command == "put":
            return command_put(link, args.file)
        if args.command == "get":
            return command_get(link, args.file)
        return command_info(link)
    finally:
        link.close()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
