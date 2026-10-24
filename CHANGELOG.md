# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- PlatformIO project for the XIAO ESP32-S3 Plus (pioarduino 55.03.312-1,
  Arduino-ESP32 3.3.12, board `seeed_xiao_esp32_s3_plus`).
- 16 MB partition table: NVS, two 4 MB OTA app slots, ~7.9 MB LittleFS,
  core dump.
- Layer skeleton (HAL, core, UI, widgets) with minimal interfaces.
- Host unit tests (`pio test -e native`) with MVG API responses as fixtures.
- MVG departures: data model, line selection and processing of the API
  response (messages, duplicates, early termination, split trains incl.
  coupled regional trains, rail replacement services, delay of combined
  trips, line filter), ported from the MVG departure display. New: departures
  are sorted by planned time plus delay.
- ArduinoJson 7.4.3 (MIT) for reading API responses from a stream with a
  filter.
- Real MVG API responses from 2026-10-19 as test fixtures.
- Rail replacement buses without a line name are treated like those for
  regional trains: shown as soon as any regional train is selected.
- Due-based scheduler: intervals aligned to the wall clock (MVG at second 1
  of every minute), retries after failures, manual refresh, age of the last
  successful update.
- Time helpers: 64-bit monotonic clock, POSIX time zones with syntax check,
  local time, minutes until a departure.
- MVG time column rules from the MVG departure display: early departures,
  delay text, minutes mode, cancelled departures.
- JSON configuration file in LittleFS with defaults, validation and issue
  reports, including the MVG widget section; see `docs/configuration.md`.
- UI layer: 1-bit bitmap and canvas with clipping, lines, rectangles,
  triangles, rounded rectangles and UTF-8 text; text shortening (`fitText`,
  `fitEvenly`), warning sign, line symbols, German/English texts and the
  layouts "one_plus_two" (departures 360 px) and "three_columns".
- Font generator (`tools/fontgen`) and Roboto Condensed in four sizes with
  Latin characters incl. umlauts.
- MVG departure list view: 48 px rows, minutes by default, cancellations,
  warnings, split trains.
- Screen previews from real MVG data, checked against reference images.
- MVV line symbols (S-Bahn, U-Bahn, tram, night tram) from the MVG departure
  display, drawn at double size; buses and regional trains get a generated
  box or frame.
- Overall UI design (style A): header widget with date, time and Wi-Fi
  status; tile titles in spaced capitals with an optional note, or a badge
  with a warning sign for the age of old data ("Stand 14:32"); section
  titles; messages for tiles without data or settings ("Nicht
  eingerichtet").
- MVG: departures in two freely named sections (`sections`, e.g. "Stadt" /
  "Raus"), lines assigned with their direction code; 9 rows shared evenly.
- `tools/png_strip.py`: removes metadata chunks from PNG files, so the
  reference images contain only image data.
- UI building blocks: spaced text, upper case for Latin characters, dotted
  lines, Wi-Fi symbol, generated line symbols as box, pill, frame or round
  frame; German and English names of weekdays and months.

- Display controller: due-based main loop that fetches the widgets' data,
  redraws once a minute and after updates, and refreshes only the changed
  areas of the panel, with a full refresh every `display.full_refresh_minutes`.
- Fault tolerance: disturbances of up to 60 s are bridged silently; then old
  data is shown with its age ("Stand 14:32"), widgets without data show "Keine
  Verbindung" or "Keine Daten"; a failing widget only affects its own tile.
  Departures that left more than a minute ago are no longer shown.
- Common widget interface for the header and the MVG widget; slots without a
  widget show "Nicht eingerichtet".
- `widgets::App` builds widgets, screen and controller from the
  configuration; `src/main.cpp` only provides clock and panel and loads
  `/config.json` from LittleFS.
- HAL: display interface with full and partial refresh, a simulated panel for
  the PC that writes PNG files, and a placeholder for the TRMNL panel (driver
  in phase B).
- Simulation of the complete main loop on the PC (simulated clock, MVG data
  and outages).

- Wi-Fi: credentials only in NVS, reconnection after the rules of the MVG
  departure display (pauses 2/5/15/30 s, Wi-Fi off and on after six failures
  in a row, power saving off, strongest access point).
- Improv Serial for entering the Wi-Fi credentials over USB (own
  implementation from the MVG departure display).
- Time synchronisation by NTP with the configured time zone.
- HTTPS requests with certificate check against the certificate bundle of
  the ESP32 core; JSON read from the stream with a filter (`fetchJsonStream`,
  body reader with chunk removal, 10 s timeout), JSON documents in PSRAM;
  `urlEncode`.
- MVG widget fetches departures from the MVG API every minute at second 1;
  the station name comes from the API if the configuration has none.
- Configuration over USB: commands `INKTAVLA CONFIG PUT/GET` and `INFO` on
  the serial line next to Improv; a new file is checked, saved atomically and
  the device restarts. `tools/inktavla_config.py` (pyserial 3.5,
  BSD-3-Clause, pinned in `tools/requirements.txt`) is the PC side.

- Weather widget with forecasts from Open-Meteo (CC BY 4.0, free for
  non-commercial use) for a place given by `latitude`, `longitude` and
  `name`, updated every 30 minutes: current weather with felt temperature
  and rain outlook, course of the day from 5 to 23 o'clock (symbols,
  temperature line dashed up to now, rain probability per hour) and the next
  three days; the tile title names the source ("WETTER · OPEN-METEO").
- Weather symbols (sun, partly cloudy, cloudy, rain, fog, moon, partly cloudy
  at night, snow, thunder) in 26, 32 and 50 px, drawn by `tools/icongen`.
- Canvas: thick lines, filled and outlined circles.
- Fonts: Roboto Condensed Medium 18 and 20, Bold 18 and 22, and Bold 56 for
  numbers only (`fontgen.py` can limit a font to a list of characters).
- A real Open-Meteo response for München from 2026-10-20 as test fixture.

- Calendar widget: an agenda of one calendar from its private iCal address
  (Google Calendar, Outlook, Nextcloud, iCloud), updated every 15 minutes:
  "Heute" and "Morgen" with their dates, the rest of the week one line per
  event; the next event bold with a countdown ("in 48 min", "in 4 h"),
  running events marked "läuft", events of today kept until midnight,
  all-day events as black badges ("ganztägig", "bis Fr"), the week number
  in the tile title and "+3 weitere" when not everything fits.
- Own streaming iCalendar reader (RFC 5545): unfolds lines, skips values
  that are not needed and drops events outside the coming days while the
  file arrives (up to 8 MB, at most 400 events kept, events and titles in
  PSRAM). Supports recurrence rules (daily to yearly with INTERVAL, COUNT,
  UNTIL, WKST, BYDAY with ordinals, BYMONTHDAY, BYMONTH, BYSETPOS),
  excluded dates, moved and cancelled occurrences, all-day and multi-day
  events and time zones from VTIMEZONE.
- Secrets of widgets in NVS: commands `INKTAVLA SECRET SET <name>` and
  `INKTAVLA SECRET CLEAR <name>` on the serial line (no command reads them
  back, replies never repeat them); `tools/inktavla_config.py calendar-url`
  asks for the calendar address without showing it; `INFO` reports
  `calendar_url=set`. An address put into `config.json` by mistake is
  reported and ignored.
- HTTPS: `fetchStream` hands a response body to a reader piece by piece and
  follows redirects; `core::PsramAllocator` for containers in PSRAM.
- Font: Roboto Condensed Medium 22 (calendar entries).
- Hand-written calendar files as test fixtures; the simulation of the main
  loop includes the calendar.
- Widget registry (`widgets/registry.cpp`): one entry per widget with its ID,
  the slot sizes its view is made for and how its data source and view are
  created; `deviceServices()` provides the APIs of the device.
- Layout template "briefing": band with date and time, a sentence and a
  weather picture with today's low and high, three columns below
  (departures 264 px, calendar and weather 214 px each) filled down to the
  bottom edge. The header line with the clock is its own refresh area.
- Briefing sentence from fixed rules: greeting by the time of day, rain
  (70 % or more, with "Schirm mit"), possible rain (30-69 %), snow,
  thunderstorms, heat (30° or more), dry; German and English; an offline
  note after 60 s without a connection.
- Briefing columns: departures (one list or two sections, rows of 28 px),
  calendar (today, the next day with events, the rest of the week),
  weather (chart that keeps the temperature curve above the rain bars,
  at least 8 degrees span, five labelled hours; three days with 38 px
  symbols; credit "Open-Meteo"); small age badge and messages in the
  column style.
- Weather symbols in 38 px that fill their square.
- Building blocks for the "Briefing" layout: word wrap (`wrapText`),
  shortening with "…" (`fitTextEllipsis`), fill rule for lists
  (`rowsThatFit`, `distributeRows`), screened areas (`fillDotted`), column
  titles with arrow or location pin and a small age badge
  (`drawColumnTitle`), line symbols with square corners for the U-Bahn and
  chains of line symbols for journeys (`drawLineSymbolChain`).
- Fonts: Fraunces 1.000 (SIL OFL 1.1) in 38, 34, 24, 20 and 18 px, Roboto
  Condensed Bold 15 and Medium 13, 15 and 16; the font generator handles
  several source fonts and their variation axes.
- Symbols: weather symbols in 30 px, large weather pictures for the band
  (156 x 132 px, rain as an umbrella with drops, a snowflake for snow),
  calendar sheet (15 px) and location pin (16 px).
- Reference sheets `briefing_symbols.png` and `briefing_parts.png` with all
  new symbols, fonts and building blocks.
- Slot size types in the layouts (column, tile, narrow column). A widget on
  a slot it is not made for is reported in the configuration issues and the
  slot shows "Passt hier nicht" ("Does not fit here") instead of a clipped
  view.
- MVG: disruption messages of the shown lines above the departures, with a
  warning sign, a heading such as "Störung S3, S4" and the texts of the API
  (every text once, wrapped, at most three lines, two with sections).
- MVG: two stations. A section may have a `station` of its own (then its
  `lines` are optional) and a free `note` on the right of its title; the
  section titles get the location pin. Each station is requested once a
  minute.
- Sentence rules for the band, ranked: no connection, a cancelled departure
  of the shown lines in the next hour, disruptions, rain/snow/thunderstorms/
  heat, in the evening a big change of tomorrow's high, the next event
  (in the evening tomorrow's first), the calm weather part. In German and
  English; a part that does not fit into three lines is left out, a shorter
  one of a lower rank may still be taken.
- Layout variants for the display test, off by default: three columns of
  the same width (`layout.grid` = `"equal"`) and dotted lines between the
  columns (`layout.dividers`).

### Removed

- Style A and the layouts "one_plus_two" and "three_columns" with their
  header, tile views and tile building blocks; MVV line bitmaps; chains of
  line symbols; rain outlook and weather texts of the tile; weather symbols
  in 26, 30, 32 and 50 px; unused fonts (Roboto Condensed Bold 18, 22, 26,
  30, 56 and Medium 18, 22, 24, 26); the felt temperature in the request.

### Changed

- "Briefing" is the only layout. The setting `layout.template` is no longer
  needed (other values are reported); `layout.slots` still orders the three
  columns (default: departures, calendar, weather).
- Weather curve from the values every 15 minutes (Open-Meteo
  `minutely_15`, 24 hours back and ahead; hourly values if they are
  missing), drawn with sub-pixel precision instead of rounded to whole
  pixels (`Canvas::drawPolyline`). The five labelled hours stay.
- Buses and rail replacement services: square frame like regional trains.
- The band also reads the departures and the calendar if they are on the
  screen; in the evening its picture follows the current weather instead of
  the weather of the whole day.
- The sun is filled in every weather symbol.
- Views draw their own title and messages (no data, settings missing); the
  views of style A keep the tile look.

- Widgets are split into a data source (fetching, data, freshness) and views
  (drawing an area of the screen). A view may read several sources, and a
  widget used in several slots fetches its data only once.
- A lost Wi-Fi connection is shown (struck-through Wi-Fi symbol, "Keine
  Verbindung") only after 60 s, like failing updates; a reconnection is
  shown at once.

- MVG view after the overall UI design: rows of 39 px (list, 10
  departures) or 38 px (sections), destination Medium 24, time Bold 26
  right-aligned, delay Bold 21 in its own column, warning sign right after
  the destination, dotted lines between rows.
- MVG: planned time plus delay is the default time format again
  (`time_format` = `"clock"`).
- MVG line symbols: generated shapes from the UI design (S-Bahn pill, tram
  and U-Bahn box, regional trains frame, buses round frame) until the MVV
  symbols are made in their final size; the MVV bitmaps can still be
  switched on in the view.
- Fonts: Roboto Condensed in the sizes of the UI design (16, 17, 20, 21, 24,
  26, 30 px) instead of 19, 23 and 27 px.
- Screen previews compare only the critical PNG chunks, so reference images
  stay valid when a tool adds metadata to them.

### Fixed

- Configuration over USB: the receive buffer of the USB serial line holds a
  whole configuration file; before, everything after the first 256 bytes was
  lost and `put` ended with "timeout while receiving".
