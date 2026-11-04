"""Writes firmware-full.bin next to firmware.bin after every device build.

firmware.bin is the app alone (for the update on the settings page and the
update button). firmware-full.bin is the whole flash image from offset 0:
bootloader, partition table, boot_app0 (OTA data) and the app. It is meant
for a first installation, e.g. with the web installer (installer/).

The image itself is made by the platform: pioarduino merges the images
after every build with its esptool ("merge-bin") into
firmware.factory.bin, with the offsets of this build (FLASH_EXTRA_IMAGES,
ESP32_APP_OFFSET), so nothing is hard-coded here. This script checks that
image (bootloader at 0, the app of this build at its offset) and saves it
as firmware-full.bin, the name used for releases (Grundsatz 4). esptool
(tool-esptoolpy, GPL-2.0-or-later) is a build tool only; nothing of it is
part of the firmware.

Note: the gaps between the parts are filled with 0xFF, so writing this
image at offset 0 also clears the NVS partition (Wi-Fi and the calendar
address). config.json in LittleFS lies behind the app and stays.

Runs as a PlatformIO post-build script (extra_scripts in platformio.ini).
"""

import os
import shutil

Import("env")  # noqa: F821 (provided by PlatformIO)


def _offset(value):
    return int(str(value), 0)


def write_full_image(source, target, env):
    build = env.subst("$BUILD_DIR")
    app_path = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    factory_path = env.subst("$BUILD_DIR/${PROGNAME}.factory.bin")
    full_path = os.path.join(build, "firmware-full.bin")
    if os.path.exists(full_path):
        os.remove(full_path)  # never leave the image of an older build
    if not os.path.isfile(factory_path) or os.path.getmtime(factory_path) < os.path.getmtime(
        app_path
    ):
        print("full_image: firmware.factory.bin of this build is missing")
        env.Exit(1)
    with open(app_path, "rb") as f:
        app = f.read()
    with open(factory_path, "rb") as f:
        full = f.read()
    app_offset = _offset(env.subst("$ESP32_APP_OFFSET") or "0x10000")
    problems = []
    if not full or full[0] != 0xE9:
        problems.append("no bootloader at offset 0")
    for image in env.get("FLASH_EXTRA_IMAGES", []):
        if _offset(image[0]) >= app_offset:
            problems.append("an extra image lies behind the app")
    if full[app_offset : app_offset + len(app)] != app:
        problems.append("the app of this build is not at 0x%x" % app_offset)
    if len(full) != app_offset + len(app):
        problems.append("unexpected size")
    if problems:
        print("full_image: firmware.factory.bin is not as expected: " + "; ".join(problems))
        env.Exit(1)
    shutil.copyfile(factory_path, full_path)
    print(
        "full_image: %s (%d bytes, app at 0x%x)"
        % (os.path.relpath(full_path, env.subst("$PROJECT_DIR")), len(full), app_offset)
    )


env.AddPostAction(  # noqa: F821
    "$BUILD_DIR/${PROGNAME}.bin",
    env.VerboseAction(write_full_image, "Writing firmware-full.bin"),  # noqa: F821
)
