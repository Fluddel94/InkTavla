# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- System log: the display notes what it does and why something fails
  (start with the reason of the last restart, Wi-Fi, clock, failed and
  recovered updates of every data source with the reason, settings page,
  firmware updates, buttons, and once an hour a summary of the updates, the
  memory and the Wi-Fi signal). KEY2 shows it on the display (system page:
  state of the device and the newest lines; KEY2 again or after two minutes
  back), the new tab "System" of the settings page shows the state, the
  whole log with a filter and a download as a text file, and
  `tools/inktavla_config.py log` reads it over USB. The system page, the tab
  and the log are always in English, whatever the language of the display,
  so they can be shared to get help. It holds fixed texts and numbers only,
  no addresses, Wi-Fi names or calendar titles. Warnings and errors survive
  a restart or a crash, nothing is written to flash. Slow updates (5 s or
  more) and buttons that had to wait are noted with their duration.
  Counters show how often the Wi-Fi connection was lost ("drops") and how
  often every data source stopped working ("fails", a series of failed
  attempts counts once), since the start and in the last 24 hours (these
  survive a restart). Before the first Wi-Fi connection after a start
  nothing is updated, so a restart leaves no failures. See
  `docs/system-log.md`.
- Calendar: emoji in event titles and locations are shown as 30 small
  pictures (all hearts, laughing, smiling, loving, crying and winking faces,
  thumbs up, party, cake, gift, star, check mark, warning, exclamation mark,
  fire, calendar, clock, phone, briefcase, house, medical, tooth, shopping
  cart, airplane, car, train, dining, coffee, soccer ball and Christmas
  tree). The pictures
  are original drawings in the size of the text; colours and skin tones make
  no difference.

### Changed

- Settings page: the tab "Gerät" holds what is set up and done on the
  device: Wi-Fi (moved from "Allgemein", with "WLAN wechseln"), backup,
  firmware update with the installed version, time zone, restart and reset.
  Uptime and memory are only in the tab "System", which shows the state.
- Web installer: also works with Firefox 151 or later on a computer (Firefox
  first asks to add a site permission add-on for the page). The page and
  `docs/updating.md` say so; uploading `firmware.bin` on the settings page
  works in every browser.

### Fixed

- Buttons are answered at once even while data is being updated: a press
  stops a running download (it is repeated right after), and the page asked
  for (system page, settings page) is drawn before the next update. Before,
  a press could wait for a whole update (several seconds for a large
  calendar, up to the timeout when a service did not answer) plus the
  refresh of the panel.
- Settings page: it no longer hangs for about 5 seconds now and then while
  loading. Browsers open a connection in advance that stays empty; the web
  server of the device waited for it and served nothing else meanwhile.
- Settings page: the tabs look like tabs (index cards with a frame, the
  selected one white with a coloured strip on top) instead of plain grey
  text, and they wrap into a second row instead of scrolling sideways
  without a hint, so the last tabs ("Gerät", "System") are visible on narrow
  windows and phones.
- Settings page: buttons show that they were pressed (they turn darker for
  a moment, also on phones), and buttons that ask the display ("Refresh",
  search, "Netze suchen", "Nach Updates suchen") show a spinner until the
  answer is there. Before, a press on "Refresh" gave no sign that anything
  happened.
- Calendar: emoji and other characters the fonts do not contain no longer
  appear as several "?" ("Datenight???"). Emoji without a picture, skin
  tones, flags and the invisible characters that belong to emoji are left
  out, and the spaces around them are merged.

## [0.2.0] - 2026-11-05

The display can now be installed without building it yourself (web
installer) and updated over Wi-Fi from the settings page. The calendar
column gets its own settings.

### Added

- Update button on the settings page (Gerät / Device): "Nach Updates suchen"
  asks GitHub for the newest release, only when pressed, and offers a newer
  version with a link to its release notes; "Installieren" downloads its
  `firmware.bin` over HTTPS and installs it like an uploaded file (check,
  second app slot, going back if it does not run). A development build is
  never replaced by an older release. See `docs/updating.md`.
- Every device build also writes `firmware-full.bin`, the whole flash image
  from offset 0 (bootloader, partition table, OTA data, app); CI keeps it
  next to `firmware.bin`. Releases carry both files.
- Web installer (`installer/`, `tools/build_installer.py`) with ESP Web
  Tools 10.4.0: flashes the display from Chrome or Edge over USB and sets up
  Wi-Fi with Improv. It is published on GitHub Pages for every release
  (`.github/workflows/pages.yml`) and can be tried locally on `localhost`.
- Settings for the calendar column, on the settings page (Kalender /
  Calendar) and in `config.json` under `widgets.calendar`: period (today,
  today and tomorrow, the next 3 or 7 days), all-day events, past events of
  today, the place of an event below its title, a hint "+ 2 weitere Termine"
  when not all events of today or of the next day fit, titles in one line
  only, words that hide events, and the update interval (5 to 360 minutes).
  Without them the column looks as before. See `docs/configuration.md`.

### Changed

- The firmware check for uploads and downloads also requires the app
  description at the start of the image, so the whole flash image
  `firmware-full.bin` (which starts with the bootloader) is refused instead
  of being written into the app slot.
- Calendar titles that are too long now fill their last line and end with
  "…" where it is full, also in the middle of a word, instead of leaving out
  the whole next word.
- Calendar column: the lines of the rest of the week show the time in front,
  like today and tomorrow (weekday only on the first line of each day, then
  time and title); "MORGEN" shows only the weekday on the right ("Mi"),
  without the date.
- Hyphenation keeps "schr", "spr" and "str" together at the start of a
  syllable ("Be-spre-chung" instead of "Besp-re-chung"); it is used for
  calendar titles and disruption texts.

## [0.1.0] - 2026-11-02

First version. It runs on the reference hardware, the Seeed TRMNL 7.5" (OG)
DIY Kit (XIAO ESP32-S3 Plus, EE04 board, 7.5" UC8179 panel, 800 x 480).

### Added

#### Device and setup

- Start screen after power-on and every restart: logo, firmware version and
  "Verbinde mit WLAN …" or "Daten werden geladen …"; it switches to the
  briefing once every data source has a first result (at the latest 20 s
  after the start).
- Wi-Fi setup over the USB cable with Improv Serial. Credentials are saved
  in NVS only after they connected. Reconnection with pauses, Wi-Fi off and
  on after repeated failures, strongest access point. Without a saved
  network, after a failed setup attempt, or if the saved network is not
  reached for 10 minutes after the start, the whole screen explains the
  Wi-Fi setup.
- Time by NTP with a configurable POSIX time zone and time server.
- Buttons: KEY1 fetches everything and does a full refresh; KEY3 opens and
  closes the settings page.
- Panel driver for the UC8179 without an extra library: partial refresh of
  the changed areas only, full refresh every `display.full_refresh_minutes`
  (default 360).
- Fault tolerance: disturbances of up to 60 s are bridged silently; then
  every column keeps its last data with an age badge ("Stand 14:32") or
  shows "Keine Verbindung" / "Keine Daten". A failing widget only affects
  its own column.
- HTTPS with certificate check against the certificate bundle of the ESP32
  core; JSON and iCal are read from the stream.
- Configuration file `/config.json` in LittleFS with defaults, validation
  and issue reports (see `docs/configuration.md`). Secrets such as the
  calendar address live only in NVS and are never read back.
- USB commands `INKTAVLA CONFIG PUT/GET`, `INFO` and `SECRET SET/CLEAR`
  with the PC tool `tools/inktavla_config.py`.
- 16 MB partition table with two OTA app slots, LittleFS and a core dump.
- `docs/troubleshooting.md`.

#### Settings page

- Settings page in the browser of a device in the same network, opened
  with KEY3, and once by itself after the start while there are no settings.
  One-time code and QR code on the display, session cookie, closes 30
  minutes after the last action or after five wrong codes.
- Tabs General, Departures - MVG, Weather - Open-Meteo, Calendar - Google
  and Device: station search or global ID, means of transport and lines per
  direction or station, weather place search, calendar address (only shown
  as "set"; replace or remove), layout, language, full refresh, time zone
  and time server; download and load `config.json`, reset, restart.
- Saving checks with the same rules as over USB, lists the problems and
  restarts the device.
- Change Wi-Fi: the new network is tested and saved only if it works.
- Firmware update: upload of `firmware.bin`, checked while receiving and
  written to the other OTA slot; the new firmware confirms itself once it
  runs properly, otherwise the bootloader goes back to the previous one.
- German and English, like the display.

#### Briefing layout

- Band at the top: date and time, a sentence in Fraunces and a weather
  picture with the temperature of now. The sentence follows the time of
  day (the whole day, then the afternoon and evening, from the evening on
  tomorrow) and reports a missing connection, failing updates and cancelled
  departures among the next ones. While the settings page is open, the band
  shows its address, the one-time code and a QR code instead.
- Three columns below: departures (280 px), calendar and weather (214 px
  each), with dotted lines between them (`layout.dividers`). `layout.slots`
  orders the columns; `layout.grid` = `"equal"` gives three equal columns.
- Columns without their settings name what is missing; a widget on a slot
  it is not made for shows "Passt hier nicht".
- German and English texts.

#### Departures (MVG)

- Departures from the unofficial MVG API, fetched every minute at second 1,
  for one list, two directions of one station (freely named sections) or
  two stations; lines chosen per means of transport and direction.
- Processing from the MVG departure display: messages, duplicates, early
  termination, split trains incl. coupled regional trains, rail replacement
  services, delay of combined trips; sorted by expected departure; a
  delayed trip listed twice is shown once.
- MVV line symbols for S-Bahn, U-Bahn, tram and night tram; frames for
  buses, regional trains and rail replacement services. Time as clock or
  minutes (`time_format`), delays, cancellations struck through with a
  warning sign.
- Disruption text under the station title (`widgets.mvg.incidents`): the
  first message of the departures shown, "+2 weitere" for the others,
  German hyphenation.

#### Weather (Open-Meteo)

- Forecast from Open-Meteo for a place by coordinates, every 30 minutes.
- Chart from 5 to 23 o'clock: smoothed temperature curve and bars with the
  amount of precipitation per hour (square-root scale).
- The next three days with symbols (rain by intensity, showers, freezing
  rain, snow, thunderstorm, fog) and high/low; credit "Open-Meteo".

#### Calendar (iCal)

- Agenda of one calendar from its private iCal address (Google Calendar,
  Outlook, Nextcloud, iCloud), every 15 minutes: today, the next day with
  events and the rest of the week; countdown to the next event, running
  events marked "läuft", all-day badges, week number, "+3 weitere".
- Own streaming iCalendar reader (RFC 5545, up to 8 MB): recurrence rules,
  excluded, moved and cancelled occurrences, all-day and multi-day events,
  time zones from VTIMEZONE.
- Long titles wrap once with hyphenation.
- A wrong address (HTTP 401/403/404/410, or no iCal file) is shown as
  "Kalender-Adresse prüfen"; network errors as "Kalender nicht erreichbar".

#### Development

- PlatformIO project (pioarduino 55.03.312-1, Arduino-ESP32 3.3.12, board
  `seeed_xiao_esp32_s3_plus`) with pinned dependencies.
- Host tests (`pio test -e native`) with API responses as fixtures,
  reference images of the screen and a simulation of the whole main loop.
- Tools: font generator (`tools/fontgen`), symbol generator
  (`tools/icongen`), `tools/embed_web.py` (packs the settings page into the
  firmware), `tools/png_strip.py`.
- Third-party components: ArduinoJson 7.4.3 (MIT), QR Code generator 1.8.0
  by Project Nayuki (MIT), pyserial 3.5 (BSD-3-Clause), fonts Roboto
  Condensed and Fraunces (SIL OFL 1.1), panel register values as in
  Seeed_GFX (FreeBSD license). Weather data by Open-Meteo (CC BY 4.0).
