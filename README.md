# E-Ink Combination Display

Firmware for a 7.5" e-paper display that shows several widgets on one
overview screen: public transport departures, weather and calendar to start
with. The image is rendered on the device itself; no server is needed.

> **Status:** early development (0.x). Nothing usable yet; the project name is
> a working title.

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

All widgets are listed in `widgets/registry.cpp` with their ID, the slot
sizes their view is made for and how their source and view are created.
Every slot of a layout has a size type (column, tile, narrow column); a
widget placed on a slot it is not made for is reported and the slot shows
"Passt hier nicht" instead of a clipped view. Adding a widget means adding
its folder and one entry in `registry.cpp` (plus its API object in
`Services` and `deviceServices()` in the same file).

### Main loop

The display controller (`core/display_controller.h`) runs the device:

- It fetches each widget's data when it is due (MVG: at second 1 of every
  minute, calendar: every 15 minutes, weather: every 30 minutes) and redraws
  the screen, at the latest once a minute for the clock.
- Short disturbances of up to 60 s are bridged silently. After that a widget
  shows its last data with the badge "Stand 14:32"; a widget that never had
  data shows "Keine Verbindung". A failing widget only affects its own tile.
  A lost Wi-Fi connection is likewise only shown (struck-through Wi-Fi
  symbol) after 60 s; reconnecting (with growing pauses between attempts,
  `core/wifi_policy.h`) goes on in the background.
- It compares the new frame with the one on the panel and refreshes only the
  changed areas; every `display.full_refresh_minutes` it does a full refresh
  against ghosting.

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

## Flash layout

Defined in `partitions.csv` (16 MB): NVS (20 KB), two 4 MB app slots for OTA
updates, ~7.9 MB LittleFS for configuration, cache and fonts, and a 64 KB
core dump partition. Changing the layout requires a full reflash via USB.

## Configuration

Nothing user-specific is compiled in.

- **Wi-Fi:** connect the device by USB and enter the credentials with
  [Improv](https://www.improv-wifi.com/) ("Connect device to Wi-Fi" in a
  browser with Web Serial, e.g. Chrome or Edge). They are stored only on the
  device (NVS) and never printed.
- **Calendar address:** the private iCal address of your calendar is a
  secret as well; it is sent over USB with
  `tools/inktavla_config.py calendar-url`, stored only in NVS and can be
  replaced or cleared but never read back.
- **Everything else** is set in a JSON file on the device, for now sent over
  USB with `tools/inktavla_config.py`, later through a settings page in the
  browser. See [docs/configuration.md](docs/configuration.md).

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
- The clock is set by NTP (`ntp_server`, default `pool.ntp.org`).
- The device only talks to the services that are configured; no telemetry.
- Departures come from the **unofficial** MVG API, which is not endorsed by
  MVG and may change at any time.

## Data sources

- **MVG** (Munich public transport): unofficial API, not endorsed by MVG.
  It may change or stop working at any time.
- **Calendar:** your own calendar, from its private iCal address (Google
  Calendar, Outlook, Nextcloud, iCloud, ...). It is only requested from the
  service that publishes it.
- **Weather data by [Open-Meteo.com](https://open-meteo.com/)**, licensed
  under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). The free
  API is for non-commercial use only; the source is named in the title of
  the weather tile.

## Third-party libraries

| Library | Version | License | Used for |
|---|---|---|---|
| [ArduinoJson](https://arduinojson.org/) | 7.4.3 | MIT | Reading API responses |
| [Unity](https://www.throwtheswitch.org/unity) | 2.6.1 | MIT | Unit tests only |
| [Roboto Condensed](https://github.com/googlefonts/roboto-classic) (font) | 3.008 | SIL OFL 1.1 | Text on the display, converted by `tools/fontgen` |
| [Fraunces](https://github.com/undercasetype/Fraunces) (font) | 1.000 | SIL OFL 1.1 (no Reserved Font Name) | Sentence and events of the "Briefing" layout, converted by `tools/fontgen` |
| [Pillow](https://python-pillow.org/) | 12.3.0 | MIT-CMU | Font generator only, not part of the firmware |

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
  recorded in [CHANGELOG.md](CHANGELOG.md).
- Code is formatted with `clang-format` (`.clang-format`, Google-based).

## License

Not decided yet; it will be set before the first public release.
