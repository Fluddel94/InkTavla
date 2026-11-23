# System log

The display keeps a log of what it does and why something fails, so problems
can be found without a USB cable and a serial monitor.

## Where to see it

- **On the display: KEY2.** The system page shows the state of the device on
  the left (version, uptime, reason of the last restart, Wi-Fi connection
  and signal with its drops, clock, memory, number of warnings and errors,
  and for every data source its state, its last update, its last failure
  and its fails) and the
  newest log lines on the right, newest first. KEY2 again, KEY3 (settings
  page) or two minutes without a press bring back the normal screen.
- **On the settings page: page "System".** The same state, the whole log with
  a filter (all, warnings and errors, errors only) and the button
  "Download (.txt)". Opening the page does not count as an action, so it
  does not keep the page open.

The system page, its page on the settings page and the log are always in English, whatever the
language of the display, so a photo, a screenshot or the downloaded file
can be shared to get help. Times of another day are shown as month-day
(`11-06 07:41`), like the date in the file.
- **Over USB:** `python tools/inktavla_config.py log inktavla-log.txt --port COM5`
  writes the log to a file, with the same first lines as the download
  (without a file name it is shown). The serial
  monitor also shows every new entry as it is made.

## What it contains

Every entry has a time (local time; before the clock is set the time since
the start, e.g. `+0:00:04`), a level, an area and a short English text:

```
2026-11-06 07:17:20 WARN  wifi     disconnected, -78 dBm, reason 200 (BEACON_TIMEOUT)
2026-11-06 07:17:29 INFO  wifi     connected after 0:00:09, -64 dBm, ch 6
2026-11-06 07:18:20 WARN  calendar fetch failed: HTTP 404 (1.2 s)
2026-11-06 07:32:10 INFO  mvg      last hour: 59 ok, 1 failed, avg 0.9, max 3.1 s
2026-11-06 07:32:10 INFO  memory   heap 179 KB (min 138, block 92), PSRAM 7598 KB
2026-11-06 07:39:50 ERROR calendar failing for 30 min (3 attempts): HTTP 404
2026-11-06 07:41:58 INFO  calendar fetch stopped for a button after 1.8 s
2026-11-06 07:41:58 INFO  keys     KEY2: system page (waited 1.3 s)
```

| Level | Used for |
|---|---|
| `INFO` | normal events: start, connection, clock set, settings page, buttons, hourly summary |
| `WARN` | something failed and is retried: a lost connection, a failed update, a wrong code |
| `ERROR` | a data source failing for 30 minutes, a crash or watchdog before the start, a rejected or failed firmware update |

| Area | Entries |
|---|---|
| `system` | start with the firmware version and the reason of the last restart (`start 0.3.0, reason: power on`); planned restarts with their reason |
| `wifi` | connected again (time without connection, signal, channel), lost and a change of the reason while reconnecting (`attempt failed, -41 dBm, reason 202 (AUTH_FAIL)`: the signal of the access point that lost or refused the device, then the reason of the Wi-Fi driver), restart of the Wi-Fi module, new network set up; hourly the signal (`signal -63 dBm (good)`) |
| `time` | the clock was set by NTP |
| `mvg`, `weather`, `calendar` | at the start the second of the minute the MVG is asked at (`updates at second 33 of every 60 s`, random per start); the first failed update with its reason (e.g. `HTTP 404`, `timed out`, a TLS error) and the duration, a change of the reason, working again after n attempts, failing for 30 minutes; an update that took 5 s or more (`slow fetch: 6.2 s`); an update stopped for a button (see below); hourly the number of updates and their average and longest duration in seconds |
| `memory` | hourly: free internal memory, its lowest value since the start (`min`), the largest free block (`block`), free PSRAM |
| `settings` | settings page opened, closed (button, page, time up), logged in, wrong code, saved, reset |
| `update` | update check, download, firmware written, rejected or failed, new firmware confirmed |
| `config` | the number of problems in the settings at the start |
| `keys` | every button press; if it had to wait a second or more for the main loop, with that time (`KEY2: system page (waited 1.3 s)`) |

The texts are short enough for one line of the system page; only reasons
from the Wi-Fi driver or the network library may be cut off there (`…`),
the download has them in full.

A data source that keeps failing is retried every few seconds; it is logged
when it starts failing, when the reason changes and when it works again, not
at every attempt. In normal operation the log grows by about 100 lines a
day.

## Counters: drops and fails

Below the Wi-Fi signal and below every data source the page shows two
numbers: since the start, and in the last 24 hours.

```
Drops 1 · Last 24h 1       (Wi-Fi)
Fails 2 · Last 24h 1       (a data source)
```

- **Drops:** how often the Wi-Fi connection was lost.
- **Fails:** how often a data source stopped working. A fail is a series of
  failed attempts until the source works again; it counts once, however
  often it is retried every few seconds (the attempts are on the page
  "System" and in the download: `830 ok, 3 failed`). A Wi-Fi drop makes
  every source fail once too.
- Nothing is counted before the first Wi-Fi connection after a start: the
  display does not try to update without it, so a restart leaves no
  `fetch failed: no network` lines and no fails.
- The last 24 hours are counted in hourly steps and kept in the same memory
  as the warnings: they survive a restart or a crash, not a power cut. The
  numbers since the start begin at 0 after every restart.

The page "System", the download (first lines) and `INKTAVLA LOG` show the
same numbers.

## Buttons during an update

The data sources are updated one after the other in the main loop; a large
calendar takes a few seconds. A button pressed meanwhile stops the running
download at once (logged as `fetch stopped for a button after 1.8 s`, not
as a failure), the button is handled, and the update is repeated right
after. Only the start of a secure connection (TLS handshake, normally well
under a second) cannot be stopped. Changing to the system page or the
settings notice then takes one refresh of the panel (about 3.5 s).

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
