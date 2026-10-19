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

The hand-written files follow the structure of real responses. Their
behaviour (message texts such as "Fährt nur bis …", cancellation flags of
early-terminating trips) is modelled on observations of the API, not on
captured responses. All `rules_*.json` times are on 2026-10-19 from 05:00 UTC.
