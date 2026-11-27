# Troubleshooting

Problems when installing, setting up or using the display. If nothing here
helps, [report the problem](#reporting-a-problem).

## Find out what the display does

The system log tells why data is missing, why the Wi-Fi drops or why the
display restarted: press KEY2 on the display, or open the page "System" on
the settings page and download the log. See [system-log.md](system-log.md).

## What the display says

Texts in a column or on the whole screen (German; in English with
`language: "en"`):

| Text | Meaning | What to do |
|---|---|---|
| "Auf der Einstellungsseite einrichten (Taste 3)" | The widget of this column is not set up yet. | Press KEY3 and set it up on the [settings page](settings-page.md). |
| "Haltestelle fehlt" | Departures without a station. | Choose a station on the page *Abfahrten*. |
| "Standort fehlt" | Weather without a place. | Choose a place on the page *Wetter*. |
| "Kalender-Adresse fehlt" | No calendar address saved. | Enter it on the page *Kalender* ([how](widgets/calendar.md#the-calendar-address)). |
| "Kalender-Adresse prüfen" | The calendar service rejected the address or sent no calendar (for example after the address was reset or the calendar no longer shared). | Copy the address again and test it on the page *Kalender*. |
| "Kalender nicht erreichbar" | The calendar service has not answered for a minute. | Nothing; the display keeps trying. If it stays, look at the [system log](system-log.md). |
| "Passt hier nicht" | The widget needs a wider column (the departures need step M). | Choose another arrangement on the page *Layout und Darstellung* ([columns](screen.md#layouts-and-columns)). |
| "Stand 14:32" next to a title | The data could not be updated for more than a minute; it is from 14:32. | Usually nothing; it goes away with the next update. |
| "Keine Verbindung" / "Keine Daten" | No data since the start. | Check the Wi-Fi; see the [system log](system-log.md). |
| Struck-through Wi-Fi symbol after the time | No Wi-Fi for more than a minute. | The display reconnects by itself; see [below](#the-display-connects-to-a-far-access-point). |
| "Noch kein WLAN eingerichtet." | No Wi-Fi saved (new or erased device). | Set it up over USB as the screen says, or with the [web installer](updating.md#web-installer). |
| "Das WLAN ist nicht erreichbar." | The saved Wi-Fi has not been reached for 10 minutes since the start (e.g. a new router). | Check the router, or set up the new Wi-Fi over USB. |
| "WLAN-Verbindung fehlgeschlagen." | A Wi-Fi setup failed (e.g. a wrong password). | See [below](#wi-fi-setup-fails). |

## The installer offers no serial port

Use a USB-C cable that carries data (not a charging-only cable) and a
computer, not a phone. If the port still does not appear, hold the
**BOOT** button on the small XIAO board while plugging in the cable, then
try again; press **RESET** once after the installation.

## Starting over

On the settings page, page *Neustart und Zurücksetzen*, **Einstellungen
zurücksetzen** / **Reset settings** sets all settings to their defaults and
deletes the calendar address; Wi-Fi stays. To start from nothing, run the
[web installer](updating.md#web-installer) and choose to erase the device.

## The board keeps connecting and disconnecting after erasing the flash

After *Erase flash* there is no firmware on the XIAO ESP32-S3 Plus, and the
board keeps restarting: the serial port (e.g. `COM8`) appears and
disappears all the time, and flashing fails.

1. Put the board into its bootloader with the two tiny buttons on the XIAO
   (not the three buttons of the EE04 board): hold **BOOT**, press **RESET**
   briefly, then release **BOOT**. The port now stays.
2. Flash the firmware (PlatformIO *Upload*, or the web installer).
3. Press **RESET** once to start the new firmware.

## Wi-Fi setup (Improv) talks to the wrong device

When several devices are connected to the computer by USB, the Wi-Fi setup
in the browser may not ask which one to use and pick a serial port that was
allowed before.

- Connect only the display you want to set up while you set up its Wi-Fi.
- If the wrong port is still used, remove the permission in the browser:
  in Chrome or Edge, the site settings of the setup page (icon left of the
  address) → *Serial ports*, or `chrome://settings/content/serialPorts`
  (Edge: `edge://settings/content/serialPorts`). In Firefox (151 or later)
  the permission is a site permission add-on: remove it under
  `about:addons`. The next attempt asks for the port again.

## Wi-Fi setup fails

If the display cannot connect with the network and password entered
(e.g. a typing error in the password), nothing is saved. The setup page in
the browser reports the error, and on a display without a saved network the
screen shows "WLAN-Verbindung fehlgeschlagen." with the name of the network
tried. Check the password and run the setup again. A network that was saved
before stays saved and is used again.

## The display connects to a far access point

With several access points of one network (a router and a repeater or mesh
access points), the display picks the strongest one. If it still ends up
with a weak, far one, the strong one may refuse it. The system log shows
this (see [system-log.md](system-log.md)):

```
WARN  wifi     attempt failed, -41 dBm, reason 202 (AUTH_FAIL)
INFO  wifi     connected after 0:00:12, -84 dBm, ch 1
```

The access point with a good signal (-41 dBm) refused the password
(reason 202), the display then took one with a weak signal (-84 dBm on
channel 1). The network name and the addresses of the access points are
never in the system log. What to check on that router or access point:

- **WPA2/WPA3:** some routers refuse the ESP32 with WPA3 only or with a
  WPA3 setting that differs between the bands. Try "WPA2 + WPA3" (mixed
  mode) or WPA2 alone for the 2.4 GHz band.
- **2.4 GHz:** the display only uses 2.4 GHz. Band steering, which pushes
  devices to 5 or 6 GHz, and multi-link operation (MLO, Wi-Fi 7) can keep it
  off the 2.4 GHz band of that access point; turn them off for 2.4 GHz or
  use a separate 2.4 GHz network.
- After a change, restart the display (system page or settings page) and
  look at the log again.

## Reporting a problem

Open an issue on GitHub:
<https://github.com/Fluddel94/InkTavla/issues/new/choose>. Please give the
firmware version (settings page, page *Firmware*, or the system page on
KEY2), the layout and what you expected, and add the lines of the
[system log](system-log.md) around the time it happened.

**Do not post your Wi-Fi name or password, your calendar address, your own
station or address, or the code of the settings page.** The system log
never contains Wi-Fi names, passwords or addresses of services, but check
what you paste anyway.
