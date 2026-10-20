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

### Changed

- MVG: minutes until departure are now the default time format.
