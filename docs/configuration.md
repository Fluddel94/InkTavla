# Configuration

All user-specific settings live in one JSON file, `/config.json`, in the
device's flash file system (LittleFS). Nothing user-specific is compiled
into the firmware.

**Not in this file:** Wi-Fi credentials and other secrets. They are entered
at runtime (first via Improv over USB) and kept in the device's protected
storage.

How the file gets onto the device (settings page in the browser) is not
implemented yet.

## Example

[`config.example.json`](config.example.json):

```json
{
  "version": 1,
  "time_zone": "CET-1CEST,M3.5.0,M10.5.0/3",
  "language": "de",
  "ntp_server": "pool.ntp.org",
  "display": {
    "full_refresh_minutes": 60
  },
  "layout": {
    "template": "one_plus_two",
    "slots": ["mvg", "weather", "calendar"]
  },
  "widgets": {
    "mvg": {
      "station": "de:09162:10",
      "name": "Pasing",
      "transport_types": ["SBAHN", "BUS", "REGIONAL_BUS", "BAHN"],
      "lines": ["S3:B:S", "RE80:R:Z"],
      "direction": "all",
      "time_format": "minutes"
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
`display.full_refresh_minutes: must be between 1 and 1440`. Where the
reports are shown (serial monitor, settings page) follows later.

## General settings

| Setting | Type | Default | Description |
|---|---|---|---|
| `version` | number | `1` | Format version of the file. Newer versions are read as far as possible and reported. |
| `time_zone` | text | `"CET-1CEST,M3.5.0,M10.5.0/3"` | Time zone as a [POSIX TZ string](https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html) (Central European Time with daylight saving time by default). Names such as `"Europe/Berlin"` are not supported. |
| `language` | `"de"`, `"en"` | `"de"` | Language of texts on the display. |
| `ntp_server` | text | `"pool.ntp.org"` | Time server (host name or IP address). |
| `display.full_refresh_minutes` | 1–1440 | `60` | Minutes between full refreshes of the panel against ghosting; in between only changed areas are refreshed. |
| `layout.template` | `"one_plus_two"`, `"three_columns"` | `"one_plus_two"` | Screen layout: one column plus two wide tiles, or three columns. |
| `layout.slots` | list of widget IDs | `["mvg", "weather", "calendar"]` | Widget for each slot of the layout, in order (3 slots for both layouts). |

## Widgets

Each widget reads its own section under `widgets`, named by its ID.

### `mvg`: Munich public transport departures

Departures from the unofficial MVG API (Munich transport and tariff
association area, MVV). The API is not endorsed by MVG and may change at any
time.

| Setting | Type | Default | Description |
|---|---|---|---|
| `station` | text | `""` | Station as MVG `globalId`, e.g. `"de:09162:10"` (München-Pasing). Empty: widget not set up. |
| `name` | text (≤ 64 bytes) | `""` | Name shown on the display. Empty: the name from the MVG API. |
| `transport_types` | list | all | Means of transport to request: `"SBAHN"`, `"UBAHN"`, `"TRAM"`, `"BUS"`, `"REGIONAL_BUS"`, `"BAHN"` (regional trains). Rail replacement buses come with the means of transport they replace. Ignored when `lines` is set. |
| `lines` | list | `[]` (all lines) | Only show these lines, at most 16. Each entry is `"line:direction:type"`, see below. |
| `direction` | `"all"`, `"H"`, `"R"` | `"all"` | Only departures with this MVG direction code (applies to all lines, in addition to `lines`). |
| `time_format` | `"minutes"`, `"clock"` | `"minutes"` | Departure as minutes until departure (`4 min`, from 60 minutes on as clock time) or as planned time with delay (`14:32 +3`). |

Line entries (`lines`):

- **line:** as shown, spaces are ignored and case does not matter (`"RE 80"`
  = `"RE80"`).
- **direction:** `H` or `R` (MVG direction code of the line) or `B` for both.
  The codes are fixed per line and do not mean "towards the city centre";
  they will be replaced by freely named views.
- **type:** `S` S-Bahn, `U` U-Bahn, `T` tram, `B` bus, `R` regional bus,
  `Z` regional train.

Examples: `"S3:B:S"` (S3, both directions), `"RE80:R:Z"` (RE 80, direction R).

If any regional train is selected, rail replacement buses for regional trains
are shown as well (also those without a line name), because the API does not
say which line they replace.
