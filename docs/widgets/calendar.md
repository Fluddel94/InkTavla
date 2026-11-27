# Calendar (iCal)

An agenda of one calendar: today, the next day with events and the rest of
the week. The next event is also named in the sentence of the band. Widget
ID `calendar`.

## Setting it up

The widget only needs the private iCal address of your calendar. On the
settings page, page **Kalender / Calendar**, paste it into the field, press
**Adresse testen** / **Test address** and save. Where to find the address:
[below](#the-calendar-address). Everything else is optional.

## The column

The column shows today with the week number ("KW 43"; past events stay
until midnight unless switched off, "Keine Termine" without any), then the
next day that has events ("MORGEN" with the weekday on the right, or the
name of the weekday with the date) if there is room, then the following
events of the week, one line each with the weekday on the first line of
every day, the title (from the same position as the titles above) and the
time on the right, as many as fit. The widget fits in every column.

All events are in the same weight; a long title wraps once onto a second
line where there is room (today only as long as every event of today that
fits still has its line), with long words hyphenated, else it is shortened
with "…". All-day events have a small calendar symbol. In a column of step
L (367 px, band + 2) today and the next day show the end time as well
("10:00–11:30"; not for events that end on a later day).

Emoji in titles and locations are shown as small pictures for a set of 30
common ones (hearts of every colour, a few faces, thumbs up, party, cake,
gift, star, check mark, warning, exclamation mark, fire, calendar, clock,
phone, briefcase, house, medical, tooth, shopping cart, airplane, car, train,
dining, coffee, soccer ball, Christmas tree); related emoji share a picture
(for example all hearts). Skin tones are ignored. Every other emoji is left
out, because the fonts have no colour emoji.

## Settings

The settings page writes these settings into the section
`widgets.calendar` of `config.json` ([configuration.md](../configuration.md));
they can also be written by hand. Without them the column looks as
described above.

```json
"calendar": {
  "days": 7,
  "location": true,
  "more": true,
  "exclude": ["Geburtstag", "Müll"]
}
```

| Key | Values | Default | Meaning |
|---|---|---|---|
| `days` | `1`, `2`, `3`, `7` | `7` | Days of the agenda: today only, today and tomorrow, the next 3 or the next 7 days (counting today). Today and tomorrow are always there (with "Keine Termine"), later days only if they have events. |
| `all_day` | `true`, `false` | `true` | All-day events, and the later days of events that last several days. `false` leaves them out. |
| `past` | `true`, `false` | `true` | Events of today that are over stay until midnight. `false` removes them as soon as they end. |
| `location` | `true`, `false` | `false` | The place of an event (`LOCATION`) in small letters below the title, today and on the next day. |
| `more` | `true`, `false` | `false` | If not all events of today or of the next day fit, "+ 2 weitere Termine" in small letters below them, right-aligned. Past events that give way are not counted. |
| `wrap` | `true`, `false` | `true` | Long titles wrap once onto a second line where there is room. `false`: always one line, shortened with "…". |
| `exclude` | list of words | `[]` | Events whose title contains one of the words are left out, e.g. `["Geburtstag"]`. Upper and lower case do not matter, part of the title is enough. At most 10 words of at most 32 bytes. |
| `band` | `true`, `false` | `true` | Mention the next event in the sentence of the band (see [the band](../screen.md#the-band)). `false` leaves it out; the notes on failed updates stay. On the settings page: the switch at the top of the page *Kalender* ("Nächsten Termin im Satz des Bands nennen"). Only `false` is saved. |
| `refresh_minutes` | 5 to 360 | `15` | How often the calendar is fetched; changes appear with the next fetch. Every request loads the whole calendar file and blocks the device for a few seconds. |

How the column uses its room: today first gets as many events as fit in one
line each (past events give way first, then the last ones; with `more` the
hint line is kept free). The space that is left goes first to the places,
then to second title lines, from the top; they can push the next day out of
the column. The next day is filled the same way with `more`; without it,
row by row, each with its place and second line as long as they fit. The
rest of the week has one line per event and no places. A title that is too
long ends with "…" where the line is full, also in the middle of a word
("Mittagessen mit Jonas und Mare…").

## The calendar address

The private iCal address is a secret: **whoever knows it can read the whole
calendar.** It therefore never goes into `config.json` but into the
device's protected storage, like the Wi-Fi password.

1. Copy the address. In Google Calendar (in a browser): point at the
   calendar in the list on the left, click the three dots and choose
   **Einstellungen und Freigabe** / **Settings and sharing**, then under
   **Kalender integrieren** / **Integrate calendar** copy
   **Privatadresse im iCal-Format** / **Secret address in iCal format**.

   Other services (Outlook, Nextcloud, iCloud, …) call it the secret,
   private or ICS address of a calendar; look for it in the sharing or
   publishing settings of the calendar.
2. On the settings page, page **Kalender / Calendar**, paste it into the
   field. **Adresse testen** / **Test address** has the display request the
   address in the field (without saving it), or the saved one if the field
   is empty, and say whether it works and how many events lie in the next
   7 days (without the word filter), or why not: address rejected, no iCal
   file, not reachable, longer than 30 s, or the clock not set yet. The
   test counts as an action (it extends the time limit) and holds the page
   for a few seconds; the address is neither sent back nor logged.
3. Save. To remove the address, use **Adresse entfernen** / **Remove
   address** on the same page.

Over the USB cable instead (serial monitor closed):

```sh
python tools/inktavla_config.py calendar-url --port COM5
```

The script asks for the address without showing it (paste it and press
Enter). The device checks it (it must start with `https://` or
`webcal://`), saves it and restarts. `info` then shows `calendar_url=set`.
To remove it: `python tools/inktavla_config.py calendar-url --clear --port COM5`.

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

## What is read

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


## Data source and limits

- The calendar file is only requested from the service at the address
  (Google, Outlook, Nextcloud, iCloud, …); nothing of it is sent anywhere
  else.
- One calendar per display.
- At most 400 events of the coming days per file are kept.
- Every request loads the whole calendar file (up to 8 MB) and blocks the
  display for a few seconds; changes in the calendar appear with the next
  fetch (every 15 minutes by default). A request is stopped after 90
  seconds (system log: `took longer than 90 s`), which is enough for about
  7 MB.
- Series with rules not listed above show their first occurrence only.
- Only the 30 emoji listed above are shown.
