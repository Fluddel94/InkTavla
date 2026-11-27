# Contributing to InkTavla

How InkTavla is built, tested and changed. How to install and use it is in
the [README](README.md).

## Conventions

- Everything in the repository is in English.
- Commit messages follow [Conventional Commits](https://www.conventionalcommits.org/)
  (`type(scope): description`, scopes = modules such as `core`, `display`,
  `transit`, `weather`, `calendar`).
- Versions follow [Semantic Versioning](https://semver.org/); changes are
  recorded in [CHANGELOG.md](CHANGELOG.md). How a version is released:
  [docs/releasing.md](docs/releasing.md).
- Code is formatted with `clang-format` (`.clang-format`, Google-based; the
  version is pinned, see [Formatting](#formatting)).
- InkTavla is developed with the help of Claude; see
  [Contributing](README.md#contributing) in the README for what that means
  for commits and pull requests.
- Images and 3D models are committed without personal metadata
  (`python tools/png_strip.py` cleans the whole repository, `--check` only
  looks; CI runs the check on every push). Editors and other tools may embed
  details about the person who made or edited a file, which would stay in
  the history for good. PNG files keep no metadata at all (copyright notes
  are in README and `LICENSE`); other image formats (JPG, WebP, GIF) are not
  accepted, use PNG. An STL file may carry a header naming the project, the
  license and the repository (`InkTavla case - CC BY-NC-SA 4.0 -
  github.com/Fluddel94/InkTavla`); the check fails only for paths, user or
  computer names and e-mail addresses in the header (binary STL: the first
  80 bytes, ASCII STL: the name behind `solid`/`endsolid`), and cleaning
  replaces such a header by that line.

Bugs and ideas: [issues](https://github.com/Fluddel94/InkTavla/issues). For
a larger change, please open an issue first.

## Documentation

- `README.md` – for users: what InkTavla is, installing, setting up, help.
- `docs/` – for users in detail: the settings page, the screen, one page
  per widget (`docs/widgets/`), the configuration file, updating,
  troubleshooting and the system log.
- `CONTRIBUTING.md` (this file) – for contributors.
- [`docs/releasing.md`](docs/releasing.md) – how a version is released.
- `test/fixtures/README.md`, `tools/fontgen/README.md` – test data and the
  font generator.

Everything is in English. Texts of the display and of the settings page are
quoted in German with the English text where it helps
(**Speichern und neu starten** / **Save and restart**).

## Architecture

The firmware is split into layers; each layer only knows the ones below it.

| Layer | Folder | Responsibility |
|---|---|---|
| Widgets | `lib/widgets` | *What* is shown. Each widget has a data source (fetching, data model) and one or more views. |
| UI | `lib/ui` | *How* it looks: layout templates, canvas, building blocks, fonts, icons, i18n. |
| Core | `lib/core` | Scheduler, display controller, configuration, cache, Wi-Fi, time, HTTPS, logging. |
| HAL | `lib/hal` | The only layer that knows the concrete hardware (panel, buttons, battery). |

`src/main.cpp` only provides the hardware parts; `widgets::App` builds the
widgets, the screen and the display controller from the configuration.
Logic is kept free of hardware, display and network code so it can be
unit-tested on the PC.

### Widgets

A widget consists of two parts (`widgets/widget.h`):

- a **data source** fetches and keeps its data (e.g. the departures of a
  station). The display controller decides when it fetches and tracks how
  fresh its data is. One source can be read by several views;
- a **view** draws one area of the screen from the data of one or more
  sources.

All widgets are listed in `widgets/registry.cpp` with their ID, the
smallest column width their view is made for, whether they have something
for the band and how their source and view are created.
The screen follows the "Briefing" design: a band with the date, the time, a
sentence and the weather at the top and three or two columns below. The
columns only come in three widths (steps S 214, M 280 and L 367 px). A
widget placed in a column that is too narrow for it is reported and the
column shows "Passt hier nicht" instead of a clipped view. Adding a widget means adding
its folder and one entry in `registry.cpp` (plus its API object in
`Services` and `deviceServices()` in the same file).

### Screen

The screen keeps a margin of 20 px at the left, right and top and 15 px at
the bottom; they are two constants (`kMargin` and `kMarginBottom` in
`lib/ui/src/ui/layout.h`), so they can be adapted if a case covers the edge
of the panel. Below the band there are three columns of 280, 214 and
214 px (the wide one is where the departures are) or two of 367 px, with
26 px between them, separated by dotted lines that can be switched off
(`layout.dividers`); `layout.slots` chooses the widgets and, by their
number, the layout. The weather chart shows the temperature of the whole
day with weather symbols at four or six hours and a dot for now.

### Main loop

The display controller (`core/display_controller.h`) runs the device:

- It fetches each widget's data when it is due (MVG: once a minute at a
  second between 20 and 45, chosen at random at every start so that not all
  displays ask the MVG at once; calendar: every 15 minutes or as set,
  weather: every 30 minutes) and redraws the screen at second 1 of every
  minute for the clock, before any fetch. New departures are drawn with
  that redraw (one panel refresh a minute); new weather or calendar data,
  the first data and data after a failure at once.
- Short disturbances of up to 60 s are bridged silently. After that a widget
  shows its last data with the badge "Stand 14:32"; a widget that never had
  data shows "Keine Verbindung". A failing widget only affects its own column.
  A lost Wi-Fi connection is likewise only shown (struck-through Wi-Fi
  symbol) after 60 s; reconnecting (with growing pauses between attempts,
  `core/wifi_policy.h`) goes on in the background.
- It compares the new frame with the one on the panel and refreshes only the
  changed areas; every `display.full_refresh_minutes` it does a full refresh
  against ghosting.
- It notes failed and recovered updates in the system log
  (`core/syslog.h`) and sums up the updates of every source once an hour;
  the system page (KEY2) and the page "System" of the settings page show the
  log and the state of the device ([docs/system-log.md](docs/system-log.md)).

### Buttons

The three buttons of the EE04 board are read by an interrupt, so a press is
not lost while the panel refreshes or data is fetched (debouncing as in the
MVG departure display, `core/buttons.h`). Their functions are listed in one
table (`core/keys.cpp`):

| Button | Short press |
|---|---|
| KEY1 (GPIO2) | Fetch everything now and refresh the whole panel |
| KEY2 (GPIO3) | Show the system page (state and log, see [docs/system-log.md](docs/system-log.md)); again: back |
| KEY3 (GPIO5) | Open the settings page (close it when open) |

Long presses (held for 5 s) are supported but not used yet.

### Without Wi-Fi

Wi-Fi is set up over the USB cable with Improv. Without a saved network,
the screen shows the steps for that at once. A setup attempt that fails
(e.g. a wrong password) saves nothing; the screen then names the network
that was tried. If a saved network has not
been reached for 10 minutes since the start (e.g. a new router), the same
screen shows its name; the device keeps trying in the background. A
connection lost later only shows the offline hints of the normal screen
(struck-through Wi-Fi symbol, age badges).

The whole loop also runs on the PC with a simulated clock, simulated data
and a simulated panel that writes every refresh as a PNG file
(`hal::PngDisplay`, see `test/test_app_simulation`).

## Building

Requirements: [VS Code](https://code.visualstudio.com/) (or VSCodium) with the
[PlatformIO](https://platformio.org/) extension.

The project uses the community platform
[pioarduino](https://github.com/pioarduino/platform-espressif32) for
Arduino-ESP32 3.x. The platform, the toolchain and all libraries are pinned to
exact versions in `platformio.ini`; the first build downloads them.
Only direct dependencies are pinned: the packages that PlatformIO or the
tools themselves pull in are not, so the CI can turn red without a change in
the repository.

```sh
pio run -e xiao_esp32s3_plus              # build the firmware
pio run -e xiao_esp32s3_plus -t upload    # flash via USB
pio device monitor                        # serial output (115200 baud)
```

If the board is not detected for flashing, hold BOOT while plugging in USB.

A build writes `firmware.bin` (the app, for updates on the settings page) and
`firmware-full.bin` (the whole flash image, for the web installer) to
`.pio/build/xiao_esp32s3_plus/`. Installing without building, the web
installer and the update button: [docs/updating.md](docs/updating.md); the
files themselves: [below](#firmware-files-and-the-web-installer).
Updates are meant to go over the update button on the settings page; the
web installer is for the first installation and repairs and resets Wi-Fi
and the calendar address.

**Windows:** the framework package contains paths longer than 260 characters.
Enable long path support once (PowerShell as administrator), then restart
VS Code:

```powershell
New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" `
  -Name "LongPathsEnabled" -Value 1 -PropertyType DWORD -Force
```

### Unit tests

```sh
pio test -e native
```

Some tests draw complete screens with the real UI code and compare them
with reference images in `test/fixtures/ui/`; the images are also written to
`.pio/preview/` for viewing. The simulation of the main loop writes every
panel refresh to `.pio/simulation/`. After an intended change of the appearance,
update the references with `INKTAVLA_UPDATE_PREVIEWS=1 pio test -e native`
and strip metadata from them before committing with
`python tools/png_strip.py` (checks and cleans the whole repository). The
tools in `tools/` also run with the Python that comes with PlatformIO, e.g.
on Windows
`& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools\png_strip.py`.

The tests run on the PC and need a host C++ compiler (GCC or Clang; on
Windows e.g. MinGW-w64 via [MSYS2](https://www.msys2.org/), with its `bin`
folder on `PATH`). Test data lives in `test/fixtures/`.

`pio test -e native_asan` runs the same tests with AddressSanitizer and
UndefinedBehaviorSanitizer (GCC or Clang on Linux or macOS; MinGW does not
support them). They stop at memory used after it was freed, writes past an
array and overflows, which the plain tests do not notice.

On Windows, the command to strip metadata from the reference images (in
the folder of the repository):

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools\png_strip.py test\fixtures\ui
```

Without a path it checks and cleans the whole repository. If
`sim_briefing_morning.png` changed, also redraw the picture in the README
with `python tools/frame_screen.py`. The test data is
described in [test/fixtures/README.md](test/fixtures/README.md).

### Formatting

C and C++ sources are formatted with clang-format (settings in
`.clang-format`). The version is pinned in `tools/requirements.txt`, because
other versions may format differently. Install it together with the other
tool packages, e.g. into the Python that comes with PlatformIO:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m pip install -r tools\requirements.txt
```

This puts `clang-format.exe` into `%USERPROFILE%\.platformio\penv\Scripts\`.
To format on save in VS Code with exactly this version, set
`C_Cpp.clang_format_path` in the user settings to its full path
(e.g. `C:\Users\<name>\.platformio\penv\Scripts\clang-format.exe`) and
enable `editor.formatOnSave` for C and C++. Check all files from the
command line with:

```sh
clang-format --dry-run -Werror <files>
```

Third-party code in `lib/qrcodegen/` keeps its original formatting.

### Continuous integration

GitHub Actions (`.github/workflows/ci.yml`) runs on every push and pull
request to `main`: formatting check, image metadata check, a warning for AI
notes (see [Contributing](README.md#contributing) in the README), the unit tests (also with
the sanitizers, `native_asan`) and the firmware build. The run summary shows the flash and RAM usage, and the built
`firmware.bin` and `firmware-full.bin` can be downloaded from the run for
30 days. The web installer is built as a check, and published on GitHub
Pages for every release by `.github/workflows/pages.yml`.

## Tools

| Tool | Use |
|---|---|
| `tools/png_strip.py` | Removes metadata from PNG files and personal details from STL headers (`--check` only looks). |
| `tools/inktavla_config.py` | Sends `config.json` and the calendar address over USB, reads the log ([docs/configuration.md](docs/configuration.md#over-the-usb-cable)). The commands are plain text lines on the serial line, next to Improv; see `lib/core/src/core/config_command.h` for the protocol. |
| `tools/mock_device.py` | Serves the settings page with made-up data at `http://localhost:8080/?code=123456`, to try it without the display. |
| `tools/embed_web.py` | Inlines and compresses the settings page (`web/`) into `src/generated/settings_page.h` before every build (not in the repository). |
| `tools/build_installer.py` | Builds the web installer ([below](#firmware-files-and-the-web-installer)). |
| `tools/full_image.py` | Writes `firmware-full.bin` after the build. |
| `tools/frame_screen.py` | Draws `docs/images/screen-de.png` and `screen-en.png`, the pictures of the screen in the README, from the reference images `sim_briefing_morning.png` and `sim_briefing_morning_en.png`; run it when one of them changes (needs Pillow). |
| `case/source/inktavla_case.py` | Makes the STL files and pictures of the case (`python case/source/inktavla_case.py slim`, and `slim-battery`); checks and its own requirements: [case/README.md](case/README.md). |
| `tools/fontgen/`, `tools/icongen/` | Generate the bitmap fonts and the symbols ([tools/fontgen/README.md](tools/fontgen/README.md)). |
| `tools/check_ai_notes.py` | Warns about AI notes and chat session links (see [Conventions](#conventions)). |

The Python tools need the packages in `tools/requirements.txt`; the Python
that comes with PlatformIO works as well.

## Firmware files and the web installer

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

The web installer (`installer/index.html`, built by
`tools/build_installer.py`) is published on GitHub Pages for every release
by `.github/workflows/pages.yml`.

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
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools\build_installer.py --firmware .pio\build\xiao_esp32s3_plus\firmware-full.bin --version 0.0.0-test --serve
```

Then open <http://localhost:8000/> in Chrome, Edge or Firefox 151+. The site
is written to `.pio/installer/`.

A fork can build with its own repository for the update button:
`-DINKTAVLA_UPDATE_REPOSITORY=\"owner/name\"` in `build_flags`.

## Flash layout

Defined in `partitions.csv` (16 MB): NVS (20 KB), two 4 MB app slots for OTA
updates, ~7.9 MB LittleFS for the configuration (`config.json`), and a 64 KB
core dump partition. Changing the layout requires a full reflash via USB.

## Network

- Services on the internet are only requested over HTTPS, and certificates
  are checked against the certificate bundle of the ESP32 core (the common
  root CAs, built into the framework). No certificates are kept in this
  repository.
- Responses are read as a stream with a filter, never held as a whole;
  large buffers go to PSRAM. Requests time out after 10 s without data.
  Calendar files (iCalendar, up to 8 MB) are read the same way, keeping only
  the events of the coming days.
- Addresses are never written to the system log, since some of them are secret
  (the private iCal address).
- Wi-Fi reconnects on its own with growing pauses (2, 5, 15, 30 s) and
  switches itself off and on after six failures in a row; power saving is
  off and the strongest access point is used.
- The settings page is served over plain HTTP in the home network (see
  [the settings page](docs/settings-page.md#security)); it only answers while it is open. Its
  files (`web/`) are compiled into the firmware: `tools/embed_web.py`
  inlines and compresses them before every build into
  `src/generated/settings_page.h` (not in the repository).
- The clock is set by NTP (`ntp_server`, default `pool.ntp.org`).

## Releasing

See [docs/releasing.md](docs/releasing.md).
