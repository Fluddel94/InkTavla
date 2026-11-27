# Configuration file

All settings of the display live in one JSON file, `/config.json`, in the
device's flash file system (LittleFS). Nothing user-specific is compiled
into the firmware. Usually the [settings page](settings-page.md) writes the
file; this page is the reference for reading and writing it by hand, for
example to keep a backup (page **Sicherung / Backup**) or to send it over
USB.

**Not in this file:** Wi-Fi credentials and other secrets such as the
address of your calendar. They are entered at runtime (Wi-Fi via Improv over
USB or on the settings page, the calendar address on the settings page or
with `tools/inktavla_config.py calendar-url`, see
[the calendar](widgets/calendar.md#the-calendar-address)) and kept only in
the device's protected storage; they cannot be read back. A calendar
address in this file (`url`, `ical`, `ical_url` or `address` under
`widgets.calendar`) is reported and removed when the file is saved, so a
backup never contains it.

## Example

[`config.example.json`](config.example.json):

```json
{
  "version": 1,
  "time_zone": "CET-1CEST,M3.5.0,M10.5.0/3",
  "language": "de",
  "ntp_server": "pool.ntp.org",
  "display": {
    "full_refresh_minutes": 360
  },
  "layout": {
    "slots": ["mvg", "calendar", "weather"]
  },
  "widgets": {
    "mvg": {
      "station": "de:09162:10",
      "name": "Pasing",
      "transport_types": ["SBAHN", "BUS", "REGIONAL_BUS", "BAHN"],
      "lines": ["S3:B:S", "RE80:R:Z"],
      "direction": "all",
      "time_format": "clock"
    },
    "weather": {
      "latitude": 48.137,
      "longitude": 11.575,
      "name": "München"
    }
  }
}
```

Every setting is optional. A missing setting uses its default, so `{}` is a
valid configuration.

## When something is wrong

The device never stops because of the configuration:

- A missing, unreadable or broken file (invalid JSON, larger than 16 KB, not
  a JSON object) is ignored completely; all defaults apply.
- An invalid value (wrong type, out of range, unknown choice) is replaced by
  its default; the other settings still apply.
- Unknown settings (for example a typing error such as `"langauge"`) are
  ignored.

Each of these cases is reported with the path of the setting, e.g.
`display.full_refresh_minutes: must be between 1 and 1440`, in the serial
monitor at the start, as the reply of `tools/inktavla_config.py put` and on
the settings page when saving.

## Compatibility

From version 1.0.0 on, a valid `config.json` stays valid with every later
firmware of the same major version:

- Settings are not renamed or removed, and their values keep their meaning.
  If the format ever has to change, the firmware still reads the old form or
  converts it itself (that is what `version` is for); you do not have to
  edit the file.
- New settings and new values can be added at any time. An older firmware
  reports them as unknown and ignores them.
- The Wi-Fi access and the calendar address in the protected storage are
  kept over every update (not over the web installer, see
  [updating.md](updating.md)).

Not covered: the internal interface between the settings page and the
device (`/api/...`) and the exact wording of the USB commands; use the
settings page or `tools/inktavla_config.py`, which always match the
firmware.

## General settings

| Setting | Type | Default | Description |
|---|---|---|---|
| `version` | number | `1` | Format version of the file. Newer versions are read as far as possible and reported. |
| `time_zone` | text | `"CET-1CEST,M3.5.0,M10.5.0/3"` | Time zone as a [POSIX TZ string](https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html) (Central European Time with daylight saving time by default). Names such as `"Europe/Berlin"` are not supported. |
| `language` | `"de"`, `"en"` | `"de"` | Language of texts on the display. |
| `ntp_server` | text | `"pool.ntp.org"` | Time server (host name or IP address). |
| `display.full_refresh_minutes` | 1–1440 | `360` | Minutes between full refreshes of the panel against ghosting; in between only changed areas are refreshed. |
| `layout.slots` | list of 2 or 3 widget IDs | `["mvg", "calendar", "weather"]` | Widget for each column of the screen from left to right. The number of entries is the layout: three widgets below the band, or two. A widget may appear more than once (its data is fetched only once); the settings page offers every widget once. |
| `layout.dividers` | `true`, `false` | `true` | Dotted vertical lines in the middle between the columns; `false` turns them off. |

How the number of widgets decides the layout and which widget fits in
which column: [the screen](screen.md#layouts-and-columns).

## Widget settings

Each widget reads its own section under `widgets`, named by its ID.

| Section | Widget | Settings |
|---|---|---|
| `widgets.mvg` | Departures | [departures.md](widgets/departures.md#settings) |
| `widgets.weather` | Weather | [weather.md](widgets/weather.md#settings) |
| `widgets.calendar` | Calendar | [calendar.md](widgets/calendar.md#settings) |

Every widget has a setting `band` (`true` by default) that takes its part
out of the sentence of the band ([the band](screen.md#the-band)).

## Over the USB cable

The file can also be sent over the USB cable with
`tools/inktavla_config.py` (needs Python 3 and pyserial:
`pip install -r tools/requirements.txt`). Close the serial monitor first;
only one program can use the port.

```sh
python tools/inktavla_config.py put my-config.json --port COM5
python tools/inktavla_config.py get backup.json --port COM5
python tools/inktavla_config.py info --port COM5
python tools/inktavla_config.py log inktavla-log.txt --port COM5
```

Without a separate Python installation, the Python that comes with
PlatformIO works as well (pyserial is included there). In a VS Code terminal
on Windows:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools\inktavla_config.py put my-config.json --port COM5
```

`put` checks the file, sends it and lists the problems the device finds in
it. The device saves the file only if it is valid JSON (at most 16 KB); it
writes a temporary file first and then replaces `/config.json`, so a
broken transfer never leaves a half file. Then it restarts with the new
settings. `get` reads the stored file back.

The commands are plain text lines on the serial line, next to Improv; see
`lib/core/src/core/config_command.h` for the protocol.
