# Installing and updating

Three ways to get InkTavla onto the display (the Seeed TRMNL 7.5" (OG) DIY
Kit, see the [README](../README.md#reference-hardware)):

| Way | For | Keeps Wi-Fi and calendar address | Keeps settings (`config.json`) |
|---|---|---|---|
| [Web installer](#web-installer) | first installation, repair | no | yes, unless the device is erased |
| [Update button](#update-button-on-the-settings-page) | new release over Wi-Fi | yes | yes |
| [File upload](#file-upload-on-the-settings-page) | own builds, older versions | yes | yes |

Building from source and flashing with PlatformIO is described in the
README ([Building](../README.md#building)).

## Files of a build and of a release

Every device build writes two files to `.pio/build/xiao_esp32s3_plus/`
(and CI keeps both as the artifact of the run):

- `firmware.bin` – the app alone. Used by the update button and the file
  upload; it is written into the second app slot.
- `firmware-full.bin` – the whole flash image from offset 0: bootloader,
  partition table, OTA data and the app (`tools/full_image.py`; the image
  is merged by the platform with esptool, using the offsets of the build).
  Used by the web installer.

A release has exactly these two files, with exactly these names. The update
button looks for `firmware.bin`; the web installer workflow takes
`firmware-full.bin`.

## Web installer

A page with [ESP Web Tools](https://github.com/esphome/esp-web-tools) that
flashes the display over the USB cable (Web Serial) and then sets up Wi-Fi
over Improv. It is published on GitHub Pages for every release by
`.github/workflows/pages.yml`.

- **Browsers:** Chrome, Edge or Firefox from version 151 (May 2026), on a
  computer (Windows, macOS, Linux). Firefox first asks to add a *site
  permission add-on* for the page; allow it, then choose the port. Safari,
  phones and tablets cannot flash (Chrome for Android has Web Serial since
  version 148, but it lists no USB ports on ordinary phones). Uploading
  `firmware.bin` on the settings page (below) works in every browser, as it
  goes over Wi-Fi.

- **Writing `firmware-full.bin` at offset 0 clears the NVS partition**
  (the gaps between the parts are filled), so Wi-Fi access and the calendar
  address have to be entered again; the installer asks for Wi-Fi right
  away. `config.json` lies in LittleFS behind the app and stays, unless
  "erase device" is chosen.
- GitHub serves release files without CORS headers, so a browser cannot
  load them from a page on another site. The workflow therefore takes
  `firmware-full.bin` from the release and publishes it together with the
  page and the manifest. No firmware is committed to the repository.
- ESP Web Tools (Apache-2.0) is downloaded from npm in a pinned version and
  checked against its SHA-512 when the site is built; it is not kept in the
  repository and not loaded from a CDN.
- Pre-releases are not published. GitHub Pages needs a public repository
  (on the free plan); set Settings → Pages → Source to "GitHub Actions".

To try the installer locally (Web Serial works on `localhost` without
HTTPS):

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools\build_installer.py --firmware .pio\build\xiao_esp32s3_plus\firmware-full.bin --version 0.2.0-dev --serve
```

Then open <http://localhost:8000/> in Chrome, Edge or Firefox 151+. The site
is written to `.pio/installer/`.

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
  else happens. A fork can build with its own repository:
  `-DINKTAVLA_UPDATE_REPOSITORY=\"owner/name\"` in `build_flags`.

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
