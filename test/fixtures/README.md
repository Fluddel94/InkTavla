# Test fixtures

## `mvg/`

Data from the unofficial MVG API (`https://www.mvg.de/api/bgw-pt/v3/`).

| File | Content |
|---|---|
| `pasing_deps.json` | Departures at München-Pasing, captured 2026-10-03, reduced to line, transport type, line ID, destination and SEV flag |
| `pasing_lines.json` | Lines serving München-Pasing (`/lines/<globalId>`) |
| `hk_deps.json` | Departures at Holzkirchen, captured 2026-10-03, reduced as above |
| `hk_lines.json` | Lines serving Holzkirchen |
| `parse_deps.json` | Hand-written full departure records for edge cases: rail replacement services (SEV) and coupled regional trains |
| `rules_messages.json` | Hand-written: message types (INFO, INCIDENT), missing `realtime`/`infos` fields, `product` instead of `transportType`, unknown transport type |
| `rules_early_termination.json` | Hand-written: early termination (still stopping here / cancelled here / message without stop name) and a cancelled trip replaced by the real one |
| `rules_split_trains.json` | Hand-written: split trains (S1 short form, coupled regional trains), duplicates, delays of combined trips and all cases that must not be combined |
| `rules_sorting.json` | Hand-written: order by planned time plus delay |
| `*_20261019.json` | Real responses captured 2026-10-19 around 22:30 local time (limit 100, all transport types) at Holzkirchen, Gmund, Schaftlach, Donnersbergerbrücke, Feldmoching and Pasing, reduced to the fields the firmware reads. RB 57 towards München Hbf only ran to Schaftlach at the time. Covers early termination (stopping here / cancelled here), a coupled train with one part cancelled, S1 split trains, coupled regional trains and replacement buses |
| `holzkirchen_full_20261019.json` | The same Holzkirchen response, unmodified with all fields (checks the stream filter) |

The hand-written files follow the structure of real responses. Their
behaviour (message texts such as "Fährt nur bis …", cancellation flags of
early-terminating trips) is modelled on observations of the API; the
captures from 2026-10-19 confirm the early-termination cases. All
`rules_*.json` times are on 2026-10-19 from 05:00 UTC.

## `calendar/`

Hand-written iCalendar files (RFC 5545) with invented events, no real
calendar data. All times are around Tuesday, 2026-10-20 (the time of the
overall UI design, 07:42 CEST).

| File | Content |
|---|---|
| `week.ics` | Structured like an export of Google Calendar: a VTIMEZONE for Europe/Berlin, a weekly series with a long folded description and an alarm, single events in local time and UTC, an all-day series every four weeks, a series with UNTIL and EXDATE, a cancelled event, a yearly all-day event since 1950, an ended series and an old single event |
| `busy.ics` | More events than fit into the tile, long titles and multi-day events (all-day and timed) |

More cases (recurrence rules, moved and cancelled occurrences, other time
zones, folding inside UTF-8 characters) are written directly into
`test/test_calendar`.

## `ui/`

Reference images of the screen previews (`test/test_ui_preview`), drawn with
the real UI and widget code from the MVG fixtures. `briefing_symbols.png` and
`briefing_parts.png` are sheets with the symbols, fonts and building blocks
of the "Briefing" layout. Their image data is
compared byte by byte (metadata chunks that tools may add are ignored); after an intended change of the appearance, update them with
`INKTAVLA_UPDATE_PREVIEWS=1 pio test -e native -f test_ui_preview`.
Before committing reference images, remove any metadata that tools may have
added: `python tools/png_strip.py test/fixtures/ui`.
