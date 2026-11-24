# InkTavla

Firmware for a 7.5" e-paper display that shows several widgets on one
overview screen: public transport departures (MVG), weather and calendar to
start with. The image is rendered on the device itself; no server is needed.

> **Status:** early development (0.x), no release yet. Departures, weather
> and calendar run on the reference hardware; Wi-Fi is set up over USB
> (Improv), everything else on a settings page in the browser, which also
> updates the firmware. Settings and layout may still change.

## Reference hardware

[Seeed TRMNL 7.5" (OG) DIY Kit](https://wiki.seeedstudio.com/trmnl_7inch5_diy_kit_main_page/):
XIAO ESP32-S3 Plus (16 MB flash, 8 MB PSRAM), EE04 driver board and a
7.5" 800×480 monochrome panel (UC8179). This is the only hardware that is
tested. Hardware-specific code is isolated in the HAL so other boards can be
added later.

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
day with weather symbols at four or five hours and a dot for now.

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

```sh
pio run -e xiao_esp32s3_plus              # build the firmware
pio run -e xiao_esp32s3_plus -t upload    # flash via USB
pio device monitor                        # serial output (115200 baud)
```

If the board is not detected for flashing, hold BOOT while plugging in USB.

A build writes `firmware.bin` (the app, for updates on the settings page) and
`firmware-full.bin` (the whole flash image, for the web installer) to
`.pio/build/xiao_esp32s3_plus/`. Installing without building, the web
installer and the update button: [docs/updating.md](docs/updating.md).
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
`python tools/png_strip.py test/fixtures/ui`. The tools in `tools/` also
run with the Python that comes with PlatformIO, e.g. on Windows
`& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools\png_strip.py test\fixtures\ui`.

The tests run on the PC and need a host C++ compiler (GCC or Clang; on
Windows e.g. MinGW-w64 via [MSYS2](https://www.msys2.org/), with its `bin`
folder on `PATH`). Test data lives in `test/fixtures/`.

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
notes (see [Contributing](#contributing)), the unit tests and the firmware
build. The run summary shows the flash and RAM usage, and the built
`firmware.bin` and `firmware-full.bin` can be downloaded from the run for
30 days. The web installer is built as a check, and published on GitHub
Pages for every release by `.github/workflows/pages.yml`.

## Flash layout

Defined in `partitions.csv` (16 MB): NVS (20 KB), two 4 MB app slots for OTA
updates, ~7.9 MB LittleFS for configuration, cache and fonts, and a 64 KB
core dump partition. Changing the layout requires a full reflash via USB.

## Configuration

Nothing user-specific is compiled in.

- **Wi-Fi:** connect the device by USB and enter the credentials with
  [Improv](https://www.improv-wifi.com/) ("Connect device to Wi-Fi" in a
  browser with Web Serial: Chrome, Edge or Firefox 151 or later on a
  computer). They are stored only on the
  device (NVS) and never printed.
- **Settings page:** press KEY3; the display shows an address, a one-time
  code and a QR code. The page in the browser sets everything else
  (departures with station search, weather place, calendar address, layout,
  language, a new Wi-Fi, a firmware update or the update button) and closes 30 minutes after the
  last action. On a new device (no settings yet) the page opens by itself
  once Wi-Fi is connected. Plain HTTP, so use it only in your private home
  network. See [docs/configuration.md](docs/configuration.md#the-settings-page).
  Problems while flashing or setting up: [docs/troubleshooting.md](docs/troubleshooting.md).
- **Calendar address:** the private iCal address of your calendar is a
  secret as well; it is set on the settings page (or sent over USB with
  `tools/inktavla_config.py calendar-url`), stored only in NVS and can be
  replaced or cleared but never read back.
- **Everything else** is kept in a JSON file on the device (`config.json`),
  written by the settings page or sent over USB with
  `tools/inktavla_config.py`. See [docs/configuration.md](docs/configuration.md).

## Network

- Services on the internet are only requested over HTTPS, and certificates
  are checked against the certificate bundle of the ESP32 core (the common
  root CAs, built into the framework). No certificates are kept in this
  repository.
- Responses are read as a stream with a filter, never held as a whole;
  large buffers go to PSRAM. Requests time out after 10 s without data.
  Calendar files (iCalendar, up to 8 MB) are read the same way, keeping only
  the events of the coming days.
- Addresses are never written to the log, since some of them are secret
  (the private iCal address).
- Wi-Fi reconnects on its own with growing pauses (2, 5, 15, 30 s) and
  switches itself off and on after six failures in a row; power saving is
  off and the strongest access point is used.
- The settings page is served over plain HTTP in the home network (see
  [Configuration](#configuration)); it only answers while it is open. Its
  files (`web/`) are compiled into the firmware: `tools/embed_web.py`
  inlines and compresses them before every build into
  `src/generated/settings_page.h` (not in the repository).
- The clock is set by NTP (`ntp_server`, default `pool.ntp.org`).
- The device only talks to the services that are configured; no telemetry.
  The one exception is on request: the update button on the settings page
  asks GitHub for the newest release only when it is pressed
  ([docs/updating.md](docs/updating.md#update-button-on-the-settings-page)).
- Departures come from the **unofficial** MVG API, which is not affiliated
  with or endorsed by MVG, MVV or DB and may change at any time.

## Data sources

- **MVG** (Munich public transport): unofficial API, not affiliated with or
  endorsed by MVG, MVV or DB. It may change or stop working at any time.
- **Calendar:** your own calendar, from its private iCal address (Google
  Calendar, Outlook, Nextcloud, iCloud, ...). It is only requested from the
  service that publishes it.
- **Weather data by [Open-Meteo.com](https://open-meteo.com/)**, licensed
  under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). The free
  API is for non-commercial use only; the source is named in the title of
  the weather column. Changes: values are rounded, and the symbol of a day
  is computed from its hourly values.

## Third-party libraries

| Library | Version | License | Used for |
|---|---|---|---|
| [ArduinoJson](https://arduinojson.org/) | 7.4.3 | MIT | Reading API responses |
| [QR Code generator](https://www.nayuki.io/page/qr-code-generator-library) (C) | 1.8.0 | MIT | QR code of the settings notice, copied into `lib/qrcodegen` |
| [Unity](https://www.throwtheswitch.org/unity) | 2.6.1 | MIT | Unit tests only |
| [Roboto Condensed](https://github.com/googlefonts/roboto-classic) (font) | 3.008 | SIL OFL 1.1 | Text on the display, converted by `tools/fontgen` |
| [Fraunces](https://github.com/undercasetype/Fraunces) (font) | 1.000 | SIL OFL 1.1 (no Reserved Font Name) | Sentence and events of the "Briefing" layout, converted by `tools/fontgen` |
| [Pillow](https://python-pillow.org/) | 12.3.0 | MIT-CMU | Font generator only, not part of the firmware |
| [esptool](https://github.com/espressif/esptool) | 5.6.0 (pioarduino) | GPL-2.0-or-later | Merges `firmware-full.bin` during the build; a build tool, not part of the firmware |
| [ESP Web Tools](https://github.com/esphome/esp-web-tools) | 10.4.0 | Apache-2.0 | Web installer page only (bundles esptool-js, Lit, Material Web, tslib), downloaded from npm when the site is built, not part of the firmware or the repository |

MVV line symbols (S-Bahn, U-Bahn): from Wikimedia Commons ("München S1.svg"
etc. by Claus Färber, marked as public domain), converted to 1 bit. They may
be trademarks of MVV, MVG or DB. Tram and night tram symbols are our own.

Weather symbols, the large weather pictures of the "Briefing" layout and the
small symbols (calendar, location pin) are our own, drawn by `tools/icongen`
(Pillow).

## Contributing

- Everything in the repository is in English.
- Commit messages follow [Conventional Commits](https://www.conventionalcommits.org/)
  (`type(scope): description`, scopes = modules such as `core`, `display`,
  `transit`, `weather`, `calendar`).
- Versions follow [Semantic Versioning](https://semver.org/); changes are
  recorded in [CHANGELOG.md](CHANGELOG.md). How a version is released:
  [docs/releasing.md](docs/releasing.md).
- Code is formatted with `clang-format` (`.clang-format`, Google-based; the
  version is pinned, see [Formatting](#formatting)).
- InkTavla is developed with the help of Claude, Anthropic's AI assistant;
  every change is reviewed and committed by the maintainer.
- Because this is stated here once, commits, pull requests and files carry no
  AI notes of their own (no co-author lines or "generated with" footers),
  and links to AI chat sessions are never added: they can lead to private
  conversations, and once the repository is public they stay in its history
  (pull requests cannot be deleted). For contributors this is a request, not
  a requirement; CI only warns (`tools/check_ai_notes.py`).
- Images are committed without metadata (`python tools/png_strip.py <files>`,
  checked by CI). Editors and other tools may embed details about the person
  who made or edited an image, which would stay in the history for good.

## License

InkTavla is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. It is distributed in the hope that it will be useful, but without any
warranty; without even the implied warranty of merchantability or fitness for
a particular purpose. The full text is in [LICENSE](LICENSE)
(SPDX: `GPL-3.0-or-later`).

Copyright (C) 2026 Fluddel94

Exceptions:

- The fonts (Roboto Condensed, Fraunces) are under the SIL Open Font License
  1.1; the license texts are in `lib/ui/src/ui/fonts/`.
- `lib/qrcodegen` is under the MIT license (see its `README.md`).
- The MVV line symbols are public domain, but may be trademarks (see
  [Third-party libraries](#third-party-libraries)).
- Case and design files (3D models, logo artwork) are under
  [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/); they
  will be added later with a `LICENSE` of their own in their folder.
