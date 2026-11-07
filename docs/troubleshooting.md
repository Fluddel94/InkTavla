# Troubleshooting

Problems when flashing the firmware or setting up the display.

## Find out what the display does

The system log tells why data is missing, why the Wi-Fi drops or why the
display restarted: press KEY2 on the display, or open the tab "System" on
the settings page and download the log. See [system-log.md](system-log.md).

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
