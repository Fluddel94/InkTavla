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
