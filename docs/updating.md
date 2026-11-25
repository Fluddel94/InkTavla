# Installing and updating

Three ways to get InkTavla onto the display (the Seeed TRMNL 7.5" (OG) DIY
Kit, see the [README](../README.md#what-you-need)):

| Way | For | Keeps Wi-Fi and calendar address | Keeps settings (`config.json`) |
|---|---|---|---|
| [Web installer](#web-installer) | first installation, repair | no | yes, unless the device is erased |
| [Update button](#update-button-on-the-settings-page) | new release over Wi-Fi | yes | yes |
| [File upload](#file-upload-on-the-settings-page) | own builds, older versions | yes | yes |

Building from source and flashing with PlatformIO:
[CONTRIBUTING.md](../CONTRIBUTING.md#building).

## Web installer

**<https://fluddel94.github.io/InkTavla/>** flashes the newest release over
the USB cable (Web Serial) and then asks for the Wi-Fi (Improv). The page
explains the steps; in short: connect the display, press **Install
InkTavla**, choose its serial port, wait about a minute, enter the Wi-Fi.

- **Browsers:** Chrome, Edge or Firefox from version 151 (May 2026), on a
  computer (Windows, macOS, Linux). Firefox first asks to add a *site
  permission add-on* for the page; allow it, then choose the port. Safari,
  phones and tablets cannot flash (Chrome for Android has Web Serial since
  version 148, but it lists no USB ports on ordinary phones). Uploading
  `firmware.bin` on the settings page (below) works in every browser, as it
  goes over Wi-Fi.
- **The installer clears the Wi-Fi access and the calendar address** every
  time, because it writes the whole flash image (`firmware-full.bin`),
  including the protected storage. It asks for the Wi-Fi right away; enter
  the calendar address again on the settings page. The settings
  (`config.json`) stay, unless "erase device" is chosen.
- **Cable:** a USB-C cable that carries data, not a charging-only cable.
- If no port is offered, hold the BOOT button on the small XIAO board while
  plugging in the cable and try again; press RESET after the installation
  (see [troubleshooting.md](troubleshooting.md)).

## Update button on the settings page

**Gerät / Device → Nach Updates suchen / Check for updates.**

- Only when the button is pressed, the display asks GitHub for the newest
  release: `https://github.com/<repository>/releases/latest` answers with a
  redirect to the page of that release (`…/releases/tag/vX.Y.Z`), and the
  version is read from it. No GitHub API, no token, no regular checks.
  GitHub sees the address (IP) of your internet connection; nothing else is
  sent.
- A newer version is offered with a link to its release notes. The same or
  an older version is not offered; a development build such as `0.2.0-dev`
  counts as newer than `0.1.0` (Semantic Versioning, pre-releases included).
- **Installieren / Install** downloads `firmware.bin` of exactly that tag
  (`…/releases/download/vX.Y.Z/firmware.bin`) over HTTPS with certificate
  check and writes it like an uploaded file (below). The display keeps
  showing its data meanwhile.
- If the repository is private or has no release, the page says so; nothing
  else happens.

## File upload on the settings page

**Gerät / Device → Firmware-Datei wählen / Choose firmware file**, then
`firmware.bin` (from a release or from a build). This works in every
browser, also on a phone: the file goes over Wi-Fi, not over USB.

For both the button and the upload: the display checks while receiving that
it is an InkTavla firmware for the ESP32-S3 and refuses anything else; the
running firmware stays until the new one is complete. Then the display
restarts and the page opens again. The new firmware is only kept once it
runs properly (30 seconds with Wi-Fi, or 10 minutes without); if it crashes
or restarts before, the display goes back to the previous firmware by
itself. Settings, Wi-Fi and calendar address stay.
