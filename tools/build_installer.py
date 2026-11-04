"""Builds the web installer: a static site that flashes InkTavla from the browser.

The site (for GitHub Pages, see .github/workflows/pages.yml) consists of

    index.html          installer/index.html
    manifest.json       ESP Web Tools manifest with the version
    firmware-full.bin   whole flash image, written at offset 0 (tools/full_image.py)
    esp-web-tools/      ESP Web Tools, from npm (see below)

ESP Web Tools (https://github.com/esphome/esp-web-tools, Apache-2.0; it
bundles esptool-js, Lit, Material Web and tslib, all under permissive
licences) is not kept in the repository and not loaded from a CDN: this
script downloads the pinned npm package, checks its SHA-512 against the
value below and copies dist/web and the licence into the site. Visitors
only talk to the site itself.

Usage (Python standard library only):

    python tools/build_installer.py --firmware .pio/build/xiao_esp32s3_plus/firmware-full.bin \\
        --version 0.2.0-dev --out .pio/installer [--serve]

--serve then serves the site at http://localhost:8000/ for a local test
(Web Serial needs HTTPS or localhost; Chrome or Edge on a computer).
"""

import argparse
import base64
import functools
import hashlib
import http.server
import io
import json
import os
import shutil
import sys
import tarfile
import urllib.request

ESP_WEB_TOOLS_VERSION = "10.4.0"
ESP_WEB_TOOLS_URL = (
    "https://registry.npmjs.org/esp-web-tools/-/esp-web-tools-%s.tgz" % ESP_WEB_TOOLS_VERSION
)
# "dist.integrity" of the package on npm (npm view esp-web-tools@10.4.0 dist.integrity).
ESP_WEB_TOOLS_SHA512 = (
    "3pwkeFFm5Fj7UQo8SJNYK5RXrtNCpq6X9QoI6bMT4GBZWgrJqjn0YvM9ihG74BtMoSFYXfmDtkehuxe50PTMPQ=="
)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def fetch_esp_web_tools(site, cache):
    """Copies dist/web and LICENSE of the pinned package to site/esp-web-tools/."""
    tgz = os.path.join(cache, "esp-web-tools-%s.tgz" % ESP_WEB_TOOLS_VERSION)
    if not os.path.isfile(tgz):
        os.makedirs(cache, exist_ok=True)
        print("Downloading ESP Web Tools %s from npm" % ESP_WEB_TOOLS_VERSION)
        with urllib.request.urlopen(ESP_WEB_TOOLS_URL, timeout=60) as r:
            data = r.read()
        with open(tgz + ".part", "wb") as f:
            f.write(data)
        os.replace(tgz + ".part", tgz)
    with open(tgz, "rb") as f:
        data = f.read()
    digest = base64.b64encode(hashlib.sha512(data).digest()).decode()
    if digest != ESP_WEB_TOOLS_SHA512:
        os.remove(tgz)
        sys.exit("build_installer: ESP Web Tools package has the wrong checksum")
    target = os.path.join(site, "esp-web-tools")
    os.makedirs(target)
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as tar:
        for member in tar.getmembers():
            name = member.name
            if not member.isfile():
                continue
            if name == "package/LICENSE":
                out = os.path.join(target, "LICENSE")
            elif name.startswith("package/dist/web/") and "/" not in name[len("package/dist/web/") :]:
                out = os.path.join(target, name[len("package/dist/web/") :])
            else:
                continue
            with tar.extractfile(member) as src, open(out, "wb") as dst:
                dst.write(src.read())
    if not os.path.isfile(os.path.join(target, "install-button.js")):
        sys.exit("build_installer: install-button.js missing in the package")


def check_image(path):
    with open(path, "rb") as f:
        head = f.read(16)
        f.seek(0x10000)
        app = f.read(16)
    # Bootloader at 0 and an ESP32-S3 app (chip ID 9) at 0x10000.
    if len(head) < 16 or head[0] != 0xE9 or len(app) < 16 or app[0] != 0xE9 or app[12] != 9:
        sys.exit("build_installer: %s is not a whole ESP32-S3 flash image" % path)


def build(firmware, version, out):
    check_image(firmware)
    if os.path.exists(out):
        shutil.rmtree(out)
    os.makedirs(out)
    shutil.copyfile(os.path.join(ROOT, "installer", "index.html"), os.path.join(out, "index.html"))
    shutil.copyfile(firmware, os.path.join(out, "firmware-full.bin"))
    manifest = {
        "name": "InkTavla",  # the firmware reports the same name over Improv
        "version": version,
        "new_install_prompt_erase": True,
        "builds": [
            {"chipFamily": "ESP32-S3", "parts": [{"path": "firmware-full.bin", "offset": 0}]}
        ],
    }
    with open(os.path.join(out, "manifest.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")
    fetch_esp_web_tools(out, os.path.join(ROOT, ".pio", "cache-installer"))
    print("Installer for %s in %s" % (version, os.path.relpath(out)))


def serve(out, port):
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=out)
    print("Serving at http://localhost:%d/ (Ctrl+C stops)" % port)
    http.server.ThreadingHTTPServer(("127.0.0.1", port), handler).serve_forever()


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--firmware", required=True, help="firmware-full.bin")
    parser.add_argument("--version", required=True, help="e.g. 0.2.0 (shown by the installer)")
    parser.add_argument("--out", default=os.path.join(ROOT, ".pio", "installer"))
    parser.add_argument("--serve", action="store_true", help="serve at http://localhost:PORT/")
    parser.add_argument("--port", type=int, default=8000)
    args = parser.parse_args()
    build(args.firmware, args.version, args.out)
    if args.serve:
        serve(args.out, args.port)


if __name__ == "__main__":
    main()
