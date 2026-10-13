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

`src/main.cpp` wires the layers together. Logic is kept free of hardware,
display and network code so it can be unit-tested on the PC.

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
on the device; everything else will be set through a configuration file and
later a settings page in the browser.

## Data sources

- **MVG** (Munich public transport): unofficial API, not endorsed by MVG.
  It may change or stop working at any time.

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
