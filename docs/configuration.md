# Configuration

All user-specific settings live in one JSON file, `/config.json`, in the
device's flash file system (LittleFS). Nothing user-specific is compiled
into the firmware.

**Not in this file:** Wi-Fi credentials and other secrets such as the
address of your calendar. They are entered at runtime (Wi-Fi via Improv over
USB or on the settings page, the calendar address on the settings page or
with `tools/inktavla_config.py calendar-url`, see
[the calendar widget](#calendar-calendar-agenda)) and kept only in the
device's protected storage; they cannot be read back.

## The settings page

All settings below can be changed in a browser, on a phone or a computer in
the same Wi-Fi as the display:

1. Press **KEY3** on the display. The band at the top shows the address of
   the page (e.g. `192.168.178.45`), a one-time code of six digits and a QR
   code. On a new device without settings (no `config.json`, or an empty
   one, e.g. after resetting) the page opens by itself once after the
   start, as soon as Wi-Fi is connected. Until then, columns without their
   settings say what is missing and "Auf der Einstellungsseite einrichten
   (Taste 3)".
2. Scan the QR code (address and code in one), or open the address in a
   browser and enter the code.
3. Change what you like and press **Speichern und neu starten** / **Save
   and restart**. The display checks the settings with the same rules as
   below; if it finds problems, nothing is saved and the page lists them
   (with **Trotzdem speichern** / **Save anyway** invalid values use their
   defaults). After saving, the display restarts and the page opens again
   for the rest of its time with the same code.

The page closes 30 minutes after the last action on it, when you press KEY3
again or press **Schließen** / **Close**, and after five wrong codes. It
has five tabs:

- **Allgemein / General:** widgets of the columns, language, full refresh,
  the dotted lines between the columns (on by default; three columns of the
  same width can be tried out below); the current Wi-Fi network and **WLAN
  wechseln** / **Change Wi-Fi**, which tests a new network and saves it only
  if the display can connect (otherwise it goes back to the old one).
- **Abfahrten - MVG / Departures - MVG:** station search; the display name
  follows the chosen station (and can still be edited); one list, two
  directions or two stations; under every station buttons for the means of
  transport that serve it, then for every line and direction whether and
  where it is shown. At first all means of transport are on and all lines
  ticked; switching one off clears and hides its lines, switching it on
  again ticks them all (with two directions it hides its lines and restores
  the previous assignment). When all lines of a station are ticked, no line
  list is saved: the station then shows all its lines, also ones added
  later, and the limit of 16 lines does not apply. With two stations the
  display name is the title of the first one. Below the station search
  (**Ändern** / **Change**) the link **Global-ID setzen** / **Set global ID**
  opens a field for a station's global ID (e.g. `de:09162:10`) when the
  search does not find it; the display checks it with the MVG and shows its
  name.
  The example destinations come from the departures of the next 12 hours;
  the names of two directions are suggested as "Zentrum" / "Auswärts"
  ("Centre" / "Outbound" in English).
- **Wetter - Open Meteo / Weather - Open Meteo:** place search ([Open-Meteo](https://open-meteo.com/en/docs/geocoding-api)),
  display name, coordinates under **Erweitert** / **Advanced**.
- **Kalender - Google / Calendar - Google:** replace or remove the private iCal address. The
  page only shows whether one is set.
- **Gerät / Device:** download and load `config.json` (a backup; secrets are
  never in it), **Firmware aktualisieren** / **Update firmware** (see below),
  time zone and time server, restart, and **Einstellungen zurücksetzen** /
  **Reset settings** (all settings to their defaults and the calendar
  address deleted; Wi-Fi stays, so the display can still be reached; the
  page then opens again by itself after the restart).

### Updating the firmware on the settings page

**Nach Updates suchen / Check for updates** asks GitHub for the newest
release, only when pressed, and offers a newer version with a link to its
release notes; **Installieren / Install** downloads and installs it. Or
choose a file `firmware.bin` (from a release, or after a build in
`.pio/build/xiao_esp32s3_plus/`). Details:
[docs/updating.md](updating.md#update-button-on-the-settings-page).

Either way the display checks while receiving it
that it is an InkTavla firmware for the ESP32-S3 and refuses anything else;
the running firmware stays until the new one is complete. Then the display
restarts with the new firmware and the page opens again. The new firmware
is only kept once it runs properly (30 seconds with Wi-Fi, or 10 minutes
without); if it crashes or restarts before, the display goes back to the
previous firmware by itself. The settings stay in both cases.

**Security:** the page uses plain HTTP, since a device in the home network
cannot get a certificate a browser trusts. Only use it in your private home
network, not in open or shared networks, where the code, the session or a
new Wi-Fi password or calendar address could be read along. That is also
why the page only opens on request and closes again by itself.

## Over the USB cable

The file can also be sent over the USB cable with
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

## General settings

| Setting | Type | Default | Description |
|---|---|---|---|
| `version` | number | `1` | Format version of the file. Newer versions are read as far as possible and reported. |
| `time_zone` | text | `"CET-1CEST,M3.5.0,M10.5.0/3"` | Time zone as a [POSIX TZ string](https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html) (Central European Time with daylight saving time by default). Names such as `"Europe/Berlin"` are not supported. |
| `language` | `"de"`, `"en"` | `"de"` | Language of texts on the display. |
| `ntp_server` | text | `"pool.ntp.org"` | Time server (host name or IP address). |
| `display.full_refresh_minutes` | 1–1440 | `360` | Minutes between full refreshes of the panel against ghosting; in between only changed areas are refreshed. |
| `layout.slots` | list of 3 widget IDs | `["mvg", "calendar", "weather"]` | Widget for each column of the screen from left to right. A widget may appear more than once; its data is fetched only once. |
| `layout.grid` | `"standard"`, `"equal"` | `"standard"` | Widths of the columns: 280/214/214 px, or three columns of 237 px (for trying out on the display; the departure column gets narrower). |
| `layout.dividers` | `true`, `false` | `true` | Dotted vertical lines in the middle between the columns; `false` turns them off. |

The `layout.template` setting of earlier versions is no longer needed: the
screen always uses the "Briefing" layout (see below). `"briefing"` is still
accepted; another value is reported and ignored.

Every column has a size type, and each widget only goes into columns of the
sizes its view is made for. A widget in another column is reported
(`layout.slots: widget "calendar" does not fit slot 1 (wide column); it
fits: column`) and the column shows "Passt hier nicht" instead of a clipped
view; its data is not fetched there.

| Column 1 | Column 2 | Column 3 |
|---|---|---|
| wide column (280 × 260) | column (214 × 260) | column (214 × 260) |

| Widget | Fits |
|---|---|
| `mvg` | wide column |
| `weather` | column |
| `calendar` | column |

### The "Briefing" layout

The screen keeps a margin of 20 px at the left, right and top (the capitals
of the date line start 20 px from the top) and 15 px at the bottom. The
margins are two constants in the firmware (`kMargin` and `kMarginBottom` in
`lib/ui/src/ui/layout.h`), in case a case covers the edge of the panel; the
departure column gets the width that is left.

At the top a band with the date and time, a sentence and a weather picture
with the temperature of now ("Jetzt 8°"); below three columns without frames
that are filled down to the bottom edge. The sentence follows fixed rules
(no AI): a greeting by the time of day ("Guten Morgen" until 11:00, "Guten
Tag" until 18:00, then "Guten Abend"), then

1. without Wi-Fi for 60 s: "Keine Verbindung zum WLAN – die Angaben sind
   von 07:12." (shorter "Kein WLAN – Stand 07:12." if that makes room for
   the rest; and a struck-through Wi-Fi symbol after the time); with Wi-Fi,
   widgets on the screen whose updates have failed for 60 s: "Die Abfahrten
   lassen sich gerade nicht abrufen – Stand 07:12.", "Wetter und Kalender
   lassen sich gerade nicht abrufen.";
2. a cancelled departure among the next two departures of each direction
   (or station), within the next hour, at most one (the earliest): "Die S2
   um 07:54 nach Dachau fällt aus.";
3. always a sentence about the weather, by the time of day:
   - until 11:00 the whole day: "Heute wird es sonnig, bis 16°.", "Erst
     trocken, ab 16 Uhr Regen – Schirm mit. Bis 14°.", "Heute regnet es den
     ganzen Tag – Schirm mit. Höchstens 9°.", "Ab 9 Uhr schneit es, um 0°.
     Es kann glatt werden.", "Sonnig und heiß, bis 32°.";
   - then the afternoon and the evening (temperature at 20:00): "Der
     Nachmittag bleibt sonnig, am Abend 11°.", "Ab 16 Uhr regnet es –
     Schirm mit. Am Abend 9°.", "Es regnet noch bis etwa 17 Uhr, danach
     trocken. Am Abend 8°."; after sunset the evening: "Ein klarer,
     trockener Abend, später 5°.";
   - tomorrow from the first full hour after sunset, but not before 18:00
     and not after 21:00 (in Munich 18:00 in winter, 21:00 in June):
     "Morgen wird es sonnig, 4° bis 15°.", "Morgen früh Nebel, später
     sonnig, bis 12°.", "Morgen ab 10 Uhr Regen – Schirm einpacken. Bis
     11°.", "Morgen zeitweise neblig, sonst trocken, bis 11°.", and if
     tomorrow's high differs by 8° or more "Morgen wird es deutlich kälter:
     höchstens 6°, wolkig.".

   The sentence and the symbols do not contradict each other. Today the
   sentence talks about the same weather as the band picture and uses the
   same rain rule: "Schirm mit" exactly when the picture is the umbrella (a
   probability of 70 % in one of the hours ahead), "vielleicht Regen" from
   30 % or when the hours bring rain by their amounts. Tomorrow the weather
   word is the daily code of Open-Meteo, as the symbol in the weather
   column; the hours give the times, "Schirm einpacken" only if that symbol
   shows rain, and weather of the code that is only at night is
   "zeitweise". For the same dry weather there are up to three wordings; the
   date and the time of day pick one, so it does not change with every
   refresh.

Disruption messages and events are not part of the sentence; they are shown
in their columns. The parts stand side by side; the sentence has at most
three lines (Fraunces 38, else 34 px). If it does not fit, the weather
sentence leaves out the temperature, the notices before it take their
shorter form, and if it still does not fit, it is left out (the notices and
a cancellation come first). Parts of sources without fresh data are left
out. The band reads the weather section (`widgets.weather`) even if the
weather column is not on the screen, the departures only if they are on the
screen. The picture shows by daylight the weather of the rest of the
daylight hours (the umbrella with drops for likely rain, a cloud for rain
that is only possible), at night the weather of now.

The calendar column shows today (past events stay until midnight, "Keine
Termine" without any), then the next day that has events ("MORGEN" or its
weekday) if there is room, then the following events of the week, one line
each, as many as fit. The departure column shows one list (title: the
station) or the two `sections` (titles with an arrow, or with the location
pin for two stations), 28 px per row. Disruption messages of the shown lines
follow the station title: a warning sign and the texts of the API for the
departures drawn, with their lines (each once, at most three lines, two with
sections).

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
| `sections` | list of 2 sections | none (one list) | Show the departures in two freely named sections instead of one list, for two directions or two stations, see below. |
| `time_format` | `"clock"`, `"minutes"` | `"clock"` | Departure as planned time with delay (`14:32 +3`) or as minutes until departure (`4 min`, from 60 minutes on as clock time). |
| `incidents` | `true`, `false` | `true` | Show the disruption text under the station title. `false` hides only the text; the warning signs at the departures stay. On the settings page: tab *Abfahrten - MVG*. |

The former setting `note` (text on the right of a title, also in sections)
was removed; an old `note` in `config.json` is ignored without a message.

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
    {"name": "Zentrum", "lines": ["S3:H:S", "S4:H:S", "S5:H:S", "S6:H:S", "S8:H:S", "19:H:T"]},
    {"name": "Auswärts", "lines": ["S3:R:S", "S4:R:S", "S6:R:S", "S8:R:S", "14:H:T", "RB6:H:Z"]}
  ]
}
```

- Exactly two sections, each with a `name` (up to 24 bytes, shown in
  capitals) and its `lines` in the same format as above (at least one).
- Above the sections a title line shows the station (location pin and
  `name`, or the name from the API). While the data is old, the age badge
  ("Stand 14:32") stands on its right.
- Lines that are in no section are not shown; `lines` and `direction` are
  ignored.
- The MVG direction code is fixed per line and does not say which way a
  line goes: at München-Pasing tram 14 "H" leaves the city while tram 19 "H"
  goes into it, and the same code can even lead to opposite destinations on
  one line. That is why every line is assigned with its direction.
- The column has room for 6 departures with two sections (8 as one list).
  They are shared evenly (3 + 3); rows a section cannot fill go to the
  other one.
- If anything about `sections` is invalid, the departures are shown as one
  list and the problem is reported.

The same response from the API is used for both sections, so two sections do
not cause more requests.

#### Two stations

A section can show a station of its own, for example the station at the
door and one a short walk away:

```json
"mvg": {
  "station": "de:09162:5",
  "sections": [
    {"name": "Ostbahnhof", "lines": ["S3:H:S", "S4:H:S"]},
    {"name": "Leuchtenbergring", "station": "de:09162:900"}
  ]
}
```

- `station` of a section: an MVG station ID as above; without it the section
  uses the station of the widget. If both sections have one, the widget's
  `station` may be left out.
- A section with a station of its own may leave out `lines`: then it shows
  all lines of that station.
- While the data is old, the first title shows the age badge on its right.
- The settings page writes the display name as the `name` of the first
  section.
- Both titles get the location pin.
- Each station is requested once a minute, so two stations mean two
  requests a minute.

#### Disruption messages

Messages of the type "INCIDENT" that the API reports for the departures
that are drawn (not for the ones that do not fit on the display) appear
below the station title (two stations: below the title of the first one,
its rows follow the text), with a warning sign: "S2: Verspätung eines
vorausfahrenden Zuges.". The API only gives short texts without a heading.
Only the first message is shown (the one of the earliest departure drawn);
if there are more, the end of the text says how many: "+2 weitere". The
line in front ("S2: ") is left out when the departures drawn belong to one
line only; for several lines it names the lines of the message. Long
German words are hyphenated at the end of a line. The text has at most
three lines above one list and two above two sections, and gets shorter,
down to one line, so that every section keeps at least two departures. The
affected departures keep a warning sign, right before the time and in the
same place in every row; a cancelled departure has the same sign and its
time struck through. Notes on early terminations stay in the row, fare notes
are not shown. `widgets.mvg.incidents: false` hides the text only; the
warning signs and the cancellations stay.

### `weather`: Weather forecast

Forecast from [Open-Meteo](https://open-meteo.com/) for one place, updated
every 30 minutes. The column shows the course of the day from 5 to 23
o'clock (temperature curve from the hourly values, each averaged with its
neighbours so that the line runs smoothly, with five labelled hours; the amount of rain per hour as bars) and the next three
days; the band at the top uses the same data for its sentence and picture.
Temperatures are in degrees Celsius. The column title is the place (`name`)
with the location pin and the credit "Open-Meteo" on the right.

The next three days are spread evenly below the chart: weekday, a 44 px
symbol and the high and low temperature at the bottom of the column. The
symbol of each day follows the daily weather code of
Open-Meteo. Rain is shown by its intensity: one stroke below the cloud for
drizzle and slight rain, two for moderate and three for heavy rain. Showers
have the sun behind the cloud, freezing drizzle and freezing rain a snow star
between two strokes. The band always shows the umbrella for rain.

Each bar shows the precipitation (rain and snow as water) of one hour and
stands between its hour marks: Open-Meteo gives the sum of the preceding
hour, so the value of 17:00 is the bar between 16 and 17. The height follows
a square root scale, so light rain stays visible: a full bar is 5 mm per
hour or more, less than 0.1 mm shows no bar. The scale follows the day within
limits: on a day with little rain the bars are drawn up to 1.5 times higher
(the wettest hour at most at two thirds of the height); on a day with more
than 5 mm in an hour that hour is full (up to 10 mm), so the hours with
little rain are drawn lower. The
sentence in the band still uses the rain probability.

| Setting | Type | Default | Description |
|---|---|---|---|
| `latitude` | −90 to 90 | none | Latitude of the place in decimal degrees, e.g. `48.137`. |
| `longitude` | −180 to 180 | none | Longitude of the place in decimal degrees, e.g. `11.575`. |
| `name` | text (≤ 32 bytes) | `""` | Name of the place, the title of the column (without a name: "Wetter"). |

Without `latitude` and `longitude` the column shows "Standort fehlt".

To find the coordinates of a place: open it in a map service such as
[OpenStreetMap](https://www.openstreetmap.org/) and right-click it
(**Adresse anzeigen** / **Show address** shows them on the left), or use the
[place search of Open-Meteo](https://open-meteo.com/en/docs/geocoding-api).
Two or three decimals are enough; the weather models work with a grid of
about 2 km.

Open-Meteo is free for non-commercial use; the data is licensed under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/), which is why the
source ("Open-Meteo") is named in the title of the weather column (the age
badge takes its place while the data is old). The values are shown rounded,
and the curve of the day is averaged from the hourly values.

### `calendar`: Calendar agenda

An agenda of one calendar from its private iCal address, updated every 15
minutes. The address works with Google Calendar, Outlook, Nextcloud, iCloud
and other services that publish a calendar in iCalendar format.

The column shows today with the week number ("KW 43"), then the next day
that has events and the rest of the week, as described under
[The "Briefing" layout](#the-briefing-layout). All events are in the same
weight; a long title wraps once onto a second line where there is room
(today only as long as every event of today that fits still has its line),
with long words hyphenated as in the disruption text of the departures,
else it is shortened with "…". All-day events have a small calendar symbol.

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

Without an address the column shows "Kalender-Adresse fehlt". If the
server rejects the address (HTTP 401, 403, 404 or 410) or sends something
other than an iCal file (e.g. a sign-in page after the calendar was no
longer shared or the address was reset), the column shows "Kalender-Adresse
prüfen" at once and the sentence at the top asks to check the address. If
the server cannot be reached for a minute, the column shows "Kalender nicht
erreichbar"; the device keeps trying.

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
