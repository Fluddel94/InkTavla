# The settings page

Everything on the display is set up in a browser, on a phone or a computer
in the same Wi-Fi as the display. The page is in the language of the
display (German or English, page *Layout und Darstellung*). What each
setting does is described on the pages of the [widgets](../README.md#widgets)
and in [configuration.md](configuration.md).

## Opening it

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
   a file written by hand ([configuration.md](configuration.md#when-something-is-wrong)); if it finds problems, nothing is saved and the page lists them
   (with **Trotzdem speichern** / **Save anyway** invalid values use their
   defaults). After saving, the display restarts and the page opens again
   for the rest of its time with the same code.

The page closes 30 minutes after the last action on it, when you press KEY3
again or press **Schließen** / **Close**, and after five wrong codes.

One browser is logged in at a time: logging in with the code on a second
device (e.g. your phone) logs out the first one, which then says so. If
that was not you, press KEY3 to close the page; the next opening shows a
new code.

## What is on it

The page opens with a card on the state of the display: the
Wi-Fi it is connected to, the signal, the version and how long it has been
running, and for every source a dot (green: up to date; amber: not set up,
waiting or outdated; red: no data or failing again and again) with the time
of its last update. The card is asked for when the page opens and on every
change of page, not in a rhythm; looking does not extend the time limit.

Below it is the overview: a list in the groups **Anzeige** / **Display**,
**Widgets**, **Gerät** / **Device** and **Diagnose** / **Diagnostics**. Every
row opens a page of its own (the address ends in `#/mvg`, `#/wifi` …, so the
back button of the browser leads to the overview and reloading stays on
the page); **‹ Übersicht** / **‹ Overview** leads back. The widgets listed
are those in a column (in the order of the columns) and the weather, which
keeps its row without a column because the band shows it. A widget without
a symbol of its own gets a plain square. The settings of a widget without a
column stay in the file. The second line of a row tells the current
entries of its page (for example "MVG · Marienplatz · Zwei Richtungen");
a row shows at most one mark: **n Fehler** / **n errors**, **Einrichten** /
**Set up** (departures without a station, weather without a place,
calendar without an address) or **geändert** / **changed** (differs from
the settings as loaded, not saved yet). In a window at least 860 px wide
the overview stays on the left and the chosen page is shown on the right
(without a page chosen, "Layout und Darstellung"). The bar with **Speichern und neu starten** / **Save
and restart** is at the bottom of every page (on the overview only once
something is changed); it saves the changes of all pages at once. A row
whose page has an entry to correct shows **1 Fehler** / **1 error**; a
problem the display reports when saving is listed at the top of the
overview, with a link to its page. At the top of the page of a widget that
has something for the band (departures, weather, calendar) is a switch that
turns its part of the band sentence off.

- **Layout und Darstellung / Layout and display:** the arrangement of the
  screen (band + 3 or band + 2) and a sketch of the display (the band shows
  an example with the date and time of the phone; its sentence follows the
  band switches of the widgets – one widget alone with a longer example,
  with two the event shorter, as on the display –, and the weather picture becomes the
  placeholder where the display would show it): tap a place to
  choose it, then pick its widget below the sketch (with a link to that
  widget's settings); below that the language, the full refresh and the
  dotted lines between the columns. Every widget is offered once; choosing
  one that is in another column already swaps the two. With three widgets
  the wide column moves to where the departures are.
- **Abfahrten / Departures** (MVG): station search, one list, two
  directions or two stations, and the lines to show; see
  [departures](widgets/departures.md#setting-it-up).
- **Wetter / Weather** (Open-Meteo): place search and display name; see
  [weather](widgets/weather.md#setting-it-up).
- **Kalender / Calendar:** the private iCal address (enter, test, replace
  or remove; the page only shows whether one is set, see
  [the calendar](widgets/calendar.md#the-calendar-address)), and the settings
  of the column under **Anzeige** / **Display**, **Filter** and **Abruf** /
  **Updates**.
- **WLAN / Wi-Fi:** the current network (name, signal, address) and **WLAN
  wechseln** / **Change Wi-Fi**, which tests a new network and saves it only
  if the display can connect (otherwise it goes back to the old one).
- **Firmware:** the installed version and the update (see [below](#updating-the-firmware)).
- **Sicherung / Backup:** download and load `config.json` (secrets are never
  in it: loading a backup keeps the calendar address the display has; on a
  new or reset display, enter it again).
- **Zeit / Time:** time zone (also as a POSIX text) and time server.
- **Neustart und Zurücksetzen / Restart and reset:** restart, and
  **Einstellungen zurücksetzen** / **Reset settings** (all settings to their
  defaults and the calendar address deleted; Wi-Fi stays, so the display
  can still be reached; the page then opens again by itself after the
  restart).
- **System** (state and diagnosis, always English): uptime, restart, Wi-Fi
  and its drops, memory, the data sources with their fails, and the system
  log (see [system-log.md](system-log.md)).

## Updating the firmware

On the page **Firmware**, **Nach Updates suchen / Check for updates**
installs a new release over Wi-Fi, and **Firmware-Datei wählen / Choose
firmware file** installs a `firmware.bin` you downloaded. Settings, Wi-Fi
and the calendar address stay. See [updating.md](updating.md).

## Security

The page uses plain HTTP, since a device in the home network
cannot get a certificate a browser trusts. Only use it in your private home
network, not in open or shared networks, where the code, the session or a
new Wi-Fi password or calendar address could be read along. That is also
why the page only opens on request and closes again by itself.
