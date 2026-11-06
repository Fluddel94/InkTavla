# System log

The display keeps a log of what it does and why something fails, so problems
can be found without a USB cable and a serial monitor.

## Where to see it

- **On the display: KEY2.** The system page shows the state of the device on
  the left (version, uptime, reason of the last restart, Wi-Fi signal, clock,
  memory, number of warnings and errors, and for every data source its
  state, its last update and its last failure) and the newest log lines on
  the right, newest first. KEY2 again, KEY3 (settings page) or two minutes
  without a press bring back the normal screen.
- **On the settings page: tab "System".** The same state, the whole log with
  a filter (all, warnings and errors, errors only) and the button
  "Herunterladen (.txt)" / "Download (.txt)". Opening the tab does not count
  as an action, so it does not keep the page open.
- **Over USB:** `python tools/inktavla_config.py log inktavla-log.txt --port COM5`
  writes the log to a file (without a file name it is shown). The serial
  monitor also shows every new entry as it is made.

## What it contains

Every entry has a time (local time; before the clock is set the time since
the start, e.g. `+0:00:04`), a level, an area and a short English text:

```
2026-11-06 07:17:20 WARN  wifi     disconnected, reason 200 (BEACON_TIMEOUT)
2026-11-06 07:18:20 WARN  calendar fetch failed: HTTP 404 (1.2 s)
2026-11-06 07:39:50 ERROR calendar failing for 30 min (3 attempts): HTTP 404
```

| Level | Used for |
|---|---|
| `INFO` | normal events: start, connection, clock set, settings page, buttons, hourly summary |
| `WARN` | something failed and is retried: a lost connection, a failed update, a wrong code |
| `ERROR` | a data source failing for 30 minutes, a crash or watchdog before the start, a rejected or failed firmware update |

| Area | Entries |
|---|---|
| `system` | start with the firmware version and the reason of the last restart; planned restarts with their reason |
| `wifi` | connected (with signal and channel), lost (with the reason of the Wi-Fi driver), a change of the reason while reconnecting, restart of the Wi-Fi module, new network set up; hourly the signal |
| `time` | the clock was set by NTP |
| `mvg`, `weather`, `calendar` | the first failed update with its reason (e.g. `HTTP 404`, `timed out`, a TLS error) and the duration, a change of the reason, working again after n attempts, failing for 30 minutes; hourly the number of updates and their average and longest duration |
| `memory` | hourly: free internal memory, its lowest value since the start, the largest free block, free PSRAM |
| `settings` | settings page opened, closed (button, page, time up), logged in, wrong code, saved, reset |
| `update` | update check, download, firmware written, rejected or failed, new firmware confirmed |
| `config` | the number of problems in the settings at the start |
| `keys` | every button press |

A data source that keeps failing is retried every few seconds; it is logged
when it starts failing, when the reason changes and when it works again, not
at every attempt. In normal operation the log grows by about 100 lines a
day.

## What it never contains

Only fixed texts of the firmware and numbers go into the log: no web
addresses (the calendar address is secret), no Wi-Fi name or password, no IP
address, no station, no coordinates, no calendar titles and nothing else
from a response. The downloaded file can therefore be shared to get help,
for example in an issue. The problems in the settings are only counted; the
settings page shows them in detail.

## Where it is kept

- 630 entries (about a week) in PSRAM. They are lost at a restart.
- Warnings, errors and the entries of the area `system` are also copied to a
  small ring of 36 entries in RTC memory, which keeps its contents over a
  restart and a crash, but not over a power cut. After a restart they are
  the oldest entries of the log, so the lines before a crash can still be
  read. A dashed line separates the starts on the display and on the
  settings page.
- Nothing is written to flash (no wear).

## Reasons of a restart

| Reason | Meaning |
|---|---|
| `power on` | the power was switched on |
| `software restart` | the firmware restarted itself (settings saved, update, the button "Neustart"); the entry before says why |
| `USB (flashing or reset)` | new firmware over USB, or the serial monitor reset the board |
| `reset pin` | the reset button |
| `crash (panic)` | the firmware crashed (logged as an error) |
| `watchdog …` | the firmware hung and was restarted (error) |
| `voltage drop (brownout)` | the supply voltage dropped too far, e.g. a weak USB power supply (error) |
