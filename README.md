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
| Widgets | `lib/widgets` | *What* is shown. Each widget has a provider (data source), a data model and a view. |
| UI | `lib/ui` | *How* it looks: layout templates, canvas, building blocks, fonts, icons, i18n. |
| Core | `lib/core` | Scheduler, display controller, configuration, cache, Wi-Fi, time, HTTPS, logging. |
| HAL | `lib/hal` | The only layer that knows the concrete hardware (panel, buttons, battery). |

`src/main.cpp` only provides the hardware parts; `widgets::App` builds the
widgets, the screen and the display controller from the configuration.
Logic is kept free of hardware, display and network code so it can be
unit-tested on the PC.

### Main loop

The display controller (`core/display_controller.h`) runs the device:

- It fetches each widget's data when it is due (MVG: at second 1 of every
  minute) and redraws the screen, at the latest once a minute for the clock.
- Short disturbances of up to 60 s are bridged silently. After that a widget
  shows its last data with the badge "Stand 14:32"; a widget that never had
  data shows "Keine Verbindung". A failing widget only affects its own tile.
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
`python tools/png_strip.py test/fixtures/ui`.

The tests run on the PC and need a host C++ compiler (GCC or Clang; on
Windows e.g. MinGW-w64 via [MSYS2](https://www.msys2.org/), with its `bin`
folder on `PATH`). Test data lives in `test/fixtures/`.

## Flash layout

Defined in `partitions.csv` (16 MB): NVS (20 KB), two 4 MB app slots for OTA
updates, ~7.9 MB LittleFS for configuration, cache and fonts, and a 64 KB
core dump partition. Changing the layout requires a full reflash via USB.

## Configuration

Nothing user-specific is compiled in. Wi-Fi credentials are entered at
runtime (first via [Improv](https://www.improv-wifi.com/) over USB) and stored
on the device; everything else is set in a JSON file on the device, later
through a settings page in the browser. See
[docs/configuration.md](docs/configuration.md).

## Data sources

- **MVG** (Munich public transport): unofficial API, not endorsed by MVG.
  It may change or stop working at any time.

## Third-party libraries

| Library | Version | License | Used for |
|---|---|---|---|
| [ArduinoJson](https://arduinojson.org/) | 7.4.3 | MIT | Reading API responses |
| [Unity](https://www.throwtheswitch.org/unity) | 2.6.1 | MIT | Unit tests only |
| [Roboto Condensed](https://github.com/googlefonts/roboto-classic) (font) | 3.008 | SIL OFL 1.1 | Text on the display, converted by `tools/fontgen` |
| [Pillow](https://python-pillow.org/) | 12.3.0 | MIT-CMU | Font generator only, not part of the firmware |

MVV line symbols (S-Bahn, U-Bahn): from Wikimedia Commons ("München S1.svg"
etc. by Claus Färber, marked as public domain), converted to 1 bit. They may
be trademarks of MVV, MVG or DB. Tram and night tram symbols are our own.

## Contributing

- Everything in the repository is in English.
- Commit messages follow [Conventional Commits](https://www.conventionalcommits.org/)
  (`type(scope): description`, scopes = modules such as `core`, `display`,
  `transit`).
- Versions follow [Semantic Versioning](https://semver.org/); changes are
  recorded in [CHANGELOG.md](CHANGELOG.md).
- Code is formatted with `clang-format` (`.clang-format`, Google-based).

## License

Not decided yet; it will be set before the first public release.
