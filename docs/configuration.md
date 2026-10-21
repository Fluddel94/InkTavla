# Configuration

All user-specific settings live in one JSON file, `/config.json`, in the
device's flash file system (LittleFS). Nothing user-specific is compiled
into the firmware.

**Not in this file:** Wi-Fi credentials and other secrets such as the
address of your calendar. They are entered at runtime (Wi-Fi via Improv over
USB, the calendar address with `tools/inktavla_config.py calendar-url`, see
[the calendar widget](#calendar-calendar-agenda)) and kept only in the
device's protected storage; they cannot be read back.

## Putting the file onto the device

Until the settings page exists, the file is sent over the USB cable with
`tools/inktavla_config.py` (needs Python 3 and pyserial:
`pip install -r tools/requirements.txt`). Close the serial monitor first;
only one program can use the port.

```sh
python tools/inktavla_config.py put my-config.json --port COM5
python tools/inktavla_config.py get backup.json --port COM5
python tools/inktavla_config.py info --port COM5
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
| `layout.slots` | list of widget IDs | `["mvg", "weather", "calendar"]` | Widget for each slot of the layout, in order (3 slots for both layouts). A widget may appear more than once; its data is fetched only once. |

Every slot has a size type, and each widget only goes on slots of the sizes
its view is made for. A widget on another slot is reported
(`layout.slots: widget "weather" does not fit slot 2 (narrow column); it
fits: tile`) and the slot shows "Passt hier nicht" instead of a clipped view;
its data is not fetched there.

| Template | Slot 1 | Slot 2 | Slot 3 |
|---|---|---|---|
| `one_plus_two` | column (358 × 438) | tile (440 × 218) | tile (440 × 218) |
| `three_columns` | column (358 × 438) | narrow column (219 × 438) | narrow column (219 × 438) |

| Widget | Fits |
|---|---|
| `mvg` | column |
| `weather` | tile |
| `calendar` | tile |

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
| `lines` | list | `[]` (all lines) | Only show these lines, at most 16. Each entry is `"line:direction:type"`, see below. Not used with `sections`. |
| `direction` | `"all"`, `"H"`, `"R"` | `"all"` | Only departures with this MVG direction code (applies to all lines, in addition to `lines`). Not used with `sections`. |
| `sections` | list of 2 sections | none (one list) | Show the departures in two freely named sections instead of one list, see below. |
| `time_format` | `"clock"`, `"minutes"` | `"clock"` | Departure as planned time with delay (`14:32 +3`) or as minutes until departure (`4 min`, from 60 minutes on as clock time). |

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

#### Two directions (`sections`)

Instead of one list, the column can show two sections with names of your
choice, for example towards the city and away from it:

```json
"mvg": {
  "station": "de:09162:10",
  "name": "Pasing",
  "sections": [
    {"name": "Stadt", "lines": ["S3:H:S", "S4:H:S", "S5:H:S", "S6:H:S", "S8:H:S", "19:H:T"]},
    {"name": "Raus", "lines": ["S3:R:S", "S4:R:S", "S6:R:S", "S8:R:S", "14:H:T", "RB6:H:Z"]}
  ]
}
```

- Exactly two sections, each with a `name` (up to 24 bytes, shown in
  capitals) and its `lines` in the same format as above (at least one).
- Lines that are in no section are not shown; `lines` and `direction` are
  ignored.
- The MVG direction code is fixed per line and does not say which way a
  line goes: at München-Pasing tram 14 "H" leaves the city while tram 19 "H"
  goes into it, and the same code can even lead to opposite destinations on
  one line. That is why every line is assigned with its direction.
- The column has room for 9 departures with two sections. They are shared
  evenly (5 + 4); rows a section cannot fill go to the other one.
- If anything about `sections` is invalid, the departures are shown as one
  list and the problem is reported.

The same response from the API is used for both sections, so two sections do
not cause more requests.

### `weather`: Weather forecast

Forecast from [Open-Meteo](https://open-meteo.com/) for one place, updated
every 30 minutes. The tile shows the current weather with the felt
temperature and the rain outlook, the course of the day from 5 to 23 o'clock
(symbols, temperature line, rain probability per hour) and the next three
days. Temperatures are in degrees Celsius.

| Setting | Type | Default | Description |
|---|---|---|---|
| `latitude` | −90 to 90 | none | Latitude of the place in decimal degrees, e.g. `48.137`. |
| `longitude` | −180 to 180 | none | Longitude of the place in decimal degrees, e.g. `11.575`. |
| `name` | text (≤ 32 bytes) | `""` | Name of the place, shown on the right of the tile title. |

Without `latitude` and `longitude` the tile shows "Standort fehlt".

To find the coordinates of a place: open it in a map service such as
[OpenStreetMap](https://www.openstreetmap.org/) and right-click it
(**Adresse anzeigen** / **Show address** shows them on the left), or use the
[place search of Open-Meteo](https://open-meteo.com/en/docs/geocoding-api).
Two or three decimals are enough; the weather models work with a grid of
about 2 km.

Rain outlook: the highest rain probability in the next 12 hours and the hours
around it that are at most 15 points lower, e.g. "65 % 14–18 Uhr". Below 20 %
the tile shows "Kein Regen".

Open-Meteo is free for non-commercial use; the data is licensed under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/), which is why the
tile title names the source ("WETTER · OPEN-METEO").

### `calendar`: Calendar agenda

An agenda of one calendar from its private iCal address, updated every 15
minutes. The address works with Google Calendar, Outlook, Nextcloud, iCloud
and other services that publish a calendar in iCalendar format.

The tile shows "Heute" and "Morgen" with their dates and below them the rest
of the week (seven days in all, one line per event with its day). The next
event of today is bold with a countdown ("in 48 min", from one hour on
"in 4 h"); events that have started show "läuft". Events of today stay
until midnight, also when they are over. All-day events are black badges
with "ganztägig"; events over several days show their last day ("bis Fr",
"bis 30.10.", "bis 12:00"). The tile title shows the week number ("KW 43").
If not everything fits, today comes first and the rest is counted
("+3 weitere").

The section has no settings yet; the widget only needs the address.

#### Setting the address

The private iCal address is a secret: **whoever knows it can read the whole
calendar.** It therefore never goes into `config.json` but into the
device's protected storage, like the Wi-Fi password.

1. Copy the address. In Google Calendar (in a browser): point at the
   calendar in the list on the left, click the three dots and choose
   **Einstellungen und Freigabe** / **Settings and sharing**, then under
   **Kalender integrieren** / **Integrate calendar** copy
   **Privatadresse im iCal-Format** / **Secret address in iCal format**.
2. Send it to the device (connected by USB, serial monitor closed):

   ```sh
   python tools/inktavla_config.py calendar-url --port COM5
   ```

   The script asks for the address without showing it (paste it and press
   Enter). The device checks it (it must start with `https://` or
   `webcal://`), saves it and restarts. `info` then shows
   `calendar_url=set`.
3. To remove it: `python tools/inktavla_config.py calendar-url --clear --port COM5`.

There is no command to read the address back, and the device never prints
it (not even in error messages). If it may have been seen by someone else,
reset it in Google Calendar (**Zurücksetzen** / **Reset** next to the
address); the old one stops working, and you send the new one to the
device.

Without an address the tile shows "Kalender-Adresse fehlt".

#### What is read

The file is read as it arrives and never held as a whole (up to 8 MB, years
of history are fine); only events of the coming days are kept, at most
400 per file. Supported:

- Single, all-day and multi-day events; cancelled events are left out.
- Series (`RRULE`) by day, week, month and year with `INTERVAL`, `COUNT`,
  `UNTIL`, weekdays (also "second Tuesday", "last Friday"), days of the
  month, months and `BYSETPOS` (e.g. "last working day"); excluded dates
  (`EXDATE`) and single occurrences that were moved or cancelled
  (`RECURRENCE-ID`). Series with other rules (e.g. several times a day)
  only show their first occurrence.
- Time zones from the file (`VTIMEZONE`), so an event in New York appears
  at the right local time. Times without a zone and zones the file does not
  describe use the device's `time_zone`.
