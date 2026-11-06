// Settings page of InkTavla (served by the display, see
// lib/core/src/core/settings_server.h). Loads the configuration, shows it
// in forms and sends the whole configuration back when saving; the device
// checks it with the same rules as over USB and restarts.
"use strict";

// --- Texts --------------------------------------------------------------------

const TEXTS = {
  de: {
    loading: "Wird geladen …",
    loginHint: "Gib den Code ein, der gerade auf dem Display steht.",
    login: "Anmelden",
    loginQr: "Mit dem QR-Code auf dem Display entfällt dieser Schritt. Nach 5 falschen Codes schließt sich die Seite.",
    loginWrong: "Der Code stimmt nicht.",
    closedHint: "Die Einstellungsseite ist geschlossen. Zum Öffnen die Taste am Display drücken; der neue Code steht dann auf dem Display.",
    close: "Schließen",
    openUntil: "Offen bis",
    saveHint: "Änderungen gelten nach dem Speichern; das Display startet dann neu.",
    departures: "Abfahrten",
    tabGeneral: "Allgemein", tabMvg: "Abfahrten - MVG", tabWeather: "Wetter - Open Meteo", tabCalendar: "Kalender - Google", tabDevice: "Gerät",
    transportModes: "Verkehrsmittel an dieser Haltestelle",
    transportHint: "Ein Knopf wählt alle Linien des Verkehrsmittels ab und blendet sie aus; erneut gedrückt sind alle wieder gewählt. Einzelne Linien lassen sich danach in den Listen ändern.",
    transportHintDir: "Ein Knopf blendet alle Linien des Verkehrsmittels aus; erneut gedrückt gilt wieder die vorherige Zuordnung.",
    sectionDefault1: "Zentrum", sectionDefault2: "Auswärts",
    station: "Haltestelle",
    stationNone: "Noch keine Haltestelle gewählt.",
    choose: "Auswählen",
    change: "Ändern",
    search: "Suchen",
    searchPlaceholder: "Name der Haltestelle",
    searching: "Suche läuft …",
    noHits: "Nichts gefunden.",
    searchFailed: "Die Suche hat nicht geklappt. Ist das Display mit dem Internet verbunden?",
    displayName: "Anzeigename",
    displayNamePlaceholder: "wie bei der MVG",
    globalId: "Global-ID",
    globalIdHint: "Für Haltestellen, die die Suche nicht findet: die MVG-Global-ID, z. B. de:09162:10.",
    globalIdApply: "Übernehmen",
    globalIdBad: "Format: zwei Kleinbuchstaben, dann durch Doppelpunkte getrennte Teile, z. B. de:09162:10.",
    globalIdChecking: "Haltestelle wird geprüft …",
    globalIdUnknown: "Unter dieser ID hat die MVG keine Haltestelle gefunden (oder das Display ist nicht mit dem Internet verbunden).",
    globalIdSet: "Global-ID setzen",
    noLinesChosen: "Noch keine Linie gewählt.",
    firstSectionTitle: "Überschrift: der Anzeigename oben.",
    layout: "Darstellung",
    modeList: "Eine Liste",
    modeDir: "Zwei Richtungen",
    modeTwo: "Zwei Haltestellen",
    section: "Abschnitt",
    linesAndDirections: "Linien und Richtungen",
    dirHint: "Je Richtung auswählen, in welchem Abschnitt sie erscheint. Die Ziele sind Beispiele aus den Abfahrten der nächsten Stunden.",
    listHint: "Haken bei den Richtungen, die angezeigt werden. Sind alle angehakt, erscheinen auch Linien, die später hinzukommen.",
    twoHint: "Jeder Abschnitt zeigt eine Haltestelle. Sind alle angehakt, erscheinen auch Linien, die später hinzukommen.",
    secondStation: "Zweite Haltestelle",
    sectionName: "Überschrift",
    hidden: "ausblenden",
    direction: "Richtung",
    noDestinations: "keine Abfahrten in den nächsten 12 Stunden",
    inactiveLines: "Ohne Abfahrten in den nächsten 12 Stunden",
    loadLines: "Linien laden",
    loadingLines: "Linien werden geladen (dauert einige Sekunden) …",
    linesFailed: "Die Linien konnten nicht geladen werden.",
    retry: "Nochmal",
    typeS: "S-Bahn", typeU: "U-Bahn", typeT: "Tram", typeB: "Bus", typeR: "Regionalbus", typeZ: "Regionalzug",
    timeFormat: "Abfahrtszeit als",
    mvgIncidents: "Störungsmeldungen unter der Haltestelle anzeigen",
    clock: "Uhrzeit",
    minutes: "Minuten",
    mvgUnofficial: "Inoffizielle MVG-Schnittstelle; sie kann sich jederzeit ändern.",
    weather: "Wetter",
    place: "Ort",
    placeNone: "Noch kein Ort gewählt.",
    placePlaceholder: "Name des Orts",
    latitude: "Breite",
    longitude: "Länge",
    calendar: "Kalender",
    calendarSet: "Adresse gesetzt",
    calendarUnset: "keine Adresse",
    calendarRemoving: "wird beim Speichern entfernt",
    calendarReplacing: "wird beim Speichern ersetzt",
    calendarHint: "Private iCal-Adresse, z. B. aus Google Kalender („Privatadresse im iCal-Format“). Sie wird nie angezeigt und lässt sich nicht auslesen, nur ersetzen oder entfernen.",
    calendarNew: "Neue Adresse",
    calendarRemove: "Adresse entfernen",
    calShow: "Anzeige",
    calDays: "Zeitraum",
    calDaysOne: "Heute",
    calDaysTwo: "Heute und morgen",
    calDaysThree: "Nächste 3 Tage",
    calDaysWeek: "Nächste 7 Tage",
    calAllDay: "Ganztägige und mehrtägige Termine anzeigen",
    calPast: "Vergangene Termine von heute bis Mitternacht anzeigen",
    calLocation: "Ort unter dem Titel anzeigen (heute und am nächsten Tag)",
    calMore: "„+ 2 weitere Termine“ anzeigen, wenn nicht alle Termine passen",
    calWrap: "Lange Titel in zwei Zeilen",
    calSpaceHint: "Ort und zweite Zeilen kommen nur dazu, wo Platz ist; dafür kann der nächste Tag entfallen.",
    calFilter: "Filter",
    calExclude: "Termine ausblenden mit diesen Wörtern im Titel",
    calExcludeHint: "Durch Komma getrennt, z. B. „Geburtstag, Müll“. Groß- und Kleinschreibung egal, ein Teil des Titels genügt.",
    calExcludeTooMany: "Höchstens {n} Wörter.",
    calFetch: "Abruf",
    calRefresh: "Kalender abrufen alle",
    calRefreshHint: "Änderungen im Kalender erscheinen beim nächsten Abruf. Jeder Abruf lädt die ganze Kalenderdatei und hält die Anzeige einige Sekunden an.",
    display: "Anzeige",
    columns: "Spalten von links nach rechts",
    wideColumn: "Breite Spalte",
    column: "Spalte",
    widgetMvg: "Abfahrten",
    widgetWeather: "Wetter",
    widgetCalendar: "Kalender",
    language: "Sprache",
    fullRefresh: "Voll-Refresh alle",
    fullRefreshHint: "Voll-Refresh: Das ganze Bild blinkt einmal und entfernt Geisterbilder.",
    tryOut: "Zum Ausprobieren",
    gridEqual: "Drei gleich breite Spalten",
    dividers: "Gepunktete Linien zwischen den Spalten",
    wifi: "WLAN",
    wifiConnected: "Verbunden mit",
    signal: "Signal",
    address: "Adresse",
    wifiChange: "WLAN wechseln",
    wifiScan: "Netze suchen",
    wifiScanning: "Suche läuft …",
    wifiName: "Name des WLANs",
    wifiPassword: "Passwort",
    wifiHow: "So läuft es ab: Das Display trennt sich vom jetzigen WLAN und probiert das neue. Klappt es, speichert es das neue WLAN und startet neu; die Seite ist dann unter der neuen Adresse auf dem Display erreichbar. Klappt es nicht, kehrt es zum alten WLAN zurück. Diese Seite verliert dabei kurz die Verbindung.",
    wifiTest: "Neues WLAN testen",
    wifiTesting: "Das neue WLAN wird getestet …",
    wifiFailed: "Das neue WLAN hat nicht geklappt (Passwort falsch?). Das Display ist wieder mit dem alten WLAN verbunden.",
    wifiOk: "Verbunden. Das Display startet neu; die neue Adresse steht dann auf dem Display.",
    wifiNameMissing: "Bitte den Namen des WLANs angeben.",
    good: "gut", fair: "mittel", weak: "schwach",
    device: "Gerät",
    firmware: "Firmware",
    uptime: "Läuft seit",
    memory: "Freier Speicher",
    backup: "Sicherung",
    backupHint: "Die Einstellungen als Datei config.json. Kalender-Adresse und WLAN-Passwort sind nie darin.",
    download: "Herunterladen",
    upload: "Datei laden …",
    uploadLoaded: "Datei geladen. Bitte prüfen und dann speichern.",
    uploadBad: "Die Datei ist keine gültige config.json.",
    firmwareUpdate: "Firmware aktualisieren",
    updateHint: "Fragt bei GitHub nach der neuesten veröffentlichten Version – nur, wenn du den Knopf drückst. GitHub sieht dabei die Adresse (IP) deines Internetanschlusses, sonst nichts.",
    updateCheck: "Nach Updates suchen",
    updateChecking: "Wird gesucht …",
    updateAvailable: "Version {v} ist verfügbar (installiert: {i}).",
    updateUpToDate: "Die Firmware ist aktuell ({i}).",
    updateNewerInstalled: "Kein Update: Installiert ist {i}, ein Entwicklungsstand, der neuer ist als die letzte Version {v}.",
    updateNoRelease: "Es gibt noch keine veröffentlichte Version.",
    updateNotFound: "Keine Versionen gefunden – das Projekt ist (noch) nicht öffentlich.",
    updateNoConnection: "GitHub ist nicht erreichbar. Hat das Display Internet?",
    updateUnexpected: "Unerwartete Antwort von GitHub ({s}). Bitte später noch einmal versuchen.",
    updateInstall: "Installieren",
    updateNotes: "Was ist neu?",
    updateDownloading: "Wird geladen: {p} – Display nicht ausschalten.",
    firmwareHint: "Oder eine Datei firmware.bin hochladen (aus einem Release oder aus .pio\\build\\xiao_esp32s3_plus\\). In beiden Fällen prüft das Display, ob es eine InkTavla-Firmware ist, und startet danach neu. Startet die neue Version nicht richtig, kehrt es zur bisherigen zurück.",
    firmwareChoose: "Firmware-Datei wählen …",
    firmwareUploading: "Wird übertragen: {p} % – Display nicht ausschalten.",
    firmwareFailed: "Update fehlgeschlagen: {e}. Die bisherige Firmware bleibt.",
    firmwareNotBin: "Bitte die Datei firmware.bin wählen.",
    advanced: "Erweitert",
    timeZone: "Zeitzone",
    timeZoneHint: "Eigene Zeitzone als POSIX-Text, z. B. CET-1CEST,M3.5.0,M10.5.0/3.",
    tzCustom: "Eigene (POSIX-Text)",
    ntpServer: "Zeitserver",
    restart: "Neu starten",
    reset: "Einstellungen zurücksetzen",
    resetConfirm: "Alle Einstellungen auf die Standardwerte zurücksetzen und die Kalender-Adresse löschen? Das WLAN bleibt. Das Display startet danach neu.",
    resetYes: "Zurücksetzen",
    cancel: "Abbrechen",
    save: "Speichern und neu starten",
    saved: "Gespeichert.",
    unsaved: "Ungespeicherte Änderungen",
    noChanges: "Keine Änderungen",
    saving: "Wird gespeichert …",
    saveFailed: "Speichern fehlgeschlagen",
    fixFirst: "Bitte zuerst die markierten Angaben korrigieren.",
    issuesTitle: "Das Display hat Probleme gefunden",
    issuesHint: "Nichts wurde gespeichert. Mit „Trotzdem speichern“ gelten für diese Angaben die Standardwerte.",
    issuesBlocking: "Nichts wurde gespeichert. Bitte die Angaben korrigieren.",
    saveAnyway: "Trotzdem speichern",
    restarting: "Das Display startet neu. Die Seite lädt sich gleich von selbst neu …",
    restartSlow: "Das dauert länger als sonst. Falls die Seite nicht wiederkommt: Taste am Display drücken.",
    connectionLost: "Keine Verbindung zum Display.",
    required: "Bitte ausfüllen.",
    tooLong: "Zu lang (höchstens {n} Zeichen).",
    tooManyLines: "Höchstens 16 Linien je Abschnitt (Linien mit beiden Richtungen zählen einmal).",
    noLinesInSection: "Abschnitt „{name}“ hat noch keine Linie.",
    stationMissing: "Bitte eine Haltestelle wählen.",
    secondStationMissing: "Bitte die zweite Haltestelle wählen.",
    coordinatesBad: "Breite −90 bis 90, Länge −180 bis 180, beide angeben oder beide leer lassen.",
    urlBad: "Die Adresse muss mit https:// oder webcal:// beginnen.",
    days: "Tagen", hours: "Std.", minutesShort: "Min.",
    pathFile: "Datei", pathMvg: "Abfahrten", pathWeather: "Wetter", pathCalendar: "Kalender",
    pathLayout: "Anzeige", pathLanguage: "Sprache", pathTimeZone: "Zeitzone", pathNtp: "Zeitserver",
    pathCalendarUrl: "Kalender-Adresse",
    tabSystem: "System", system: "System", sysSources: "Datenquellen", sysLog: "Protokoll",
    sysLogHint: "Neueste Einträge zuerst. Es enthält nur feste Texte und Zahlen – keine Adressen, WLAN-Namen oder Kalendertitel – und lässt sich zur Fehlersuche weitergeben. Warnungen und Fehler überstehen einen Neustart, nach einem Stromausfall beginnt es neu. Am Display zeigt die Taste KEY2 dasselbe.",
    sysAll: "Alle", sysWarn: "Warnungen + Fehler", sysErrors: "Nur Fehler", sysRefresh: "Aktualisieren",
    sysDownload: "Herunterladen (.txt)", sysFirmware: "Firmware", sysUptime: "Laufzeit",
    sysRestart: "Letzter Neustart", sysUnplanned: "ungeplant", sysStart: "{n}. Start seit dem Einschalten",
    sysWifi: "WLAN", sysConnected: "verbunden, {rssi} dBm ({q})", sysNotConnected: "nicht verbunden",
    sysClock: "Uhrzeit", sysClockSet: "gestellt (NTP)", sysClockNotSet: "noch nicht gestellt",
    sysMemory: "Speicher", sysMemoryText: "{free} KB frei, niedrigster Stand {min} KB, größter Block {block} KB",
    sysPsram: "PSRAM", sysPsramText: "{free} von {size} MB frei", sysCounts: "Protokoll",
    sysCountsText: "{w} Warnungen, {e} Fehler", qExcellent: "sehr gut", qGood: "gut", qFair: "mäßig", qWeak: "schwach",
    stateOk: "aktuell", stateStale: "veraltet", stateWaiting: "wartet", stateNoData: "keine Daten", stateNotSetUp: "nicht eingerichtet",
    sysLastOk: "zuletzt {time} ({s} s)", sysFailing: "Fehler seit {time}: {problem} ({n}×)", sysLastFailure: "letzter Fehler {time}: {problem}",
    sysTotals: "{ok} ok, {failed} fehlgeschlagen seit dem Start", sysNoEntries: "Keine Einträge.", sysLoadFailed: "Konnte nicht geladen werden.",
  },
  en: {
    loading: "Loading …",
    loginHint: "Enter the code that the display shows right now.",
    login: "Log in",
    loginQr: "With the QR code on the display you can skip this step. After 5 wrong codes the page closes.",
    loginWrong: "The code is not correct.",
    closedHint: "The settings page is closed. Press the button on the display to open it; the new code then appears on the display.",
    close: "Close",
    openUntil: "Open until",
    saveHint: "Changes apply after saving; the display then restarts.",
    departures: "Departures",
    tabGeneral: "General", tabMvg: "Departures - MVG", tabWeather: "Weather - Open Meteo", tabCalendar: "Calendar - Google", tabDevice: "Device",
    transportModes: "Modes of transport at this station",
    transportHint: "A button clears and hides all lines of that mode; pressed again, they are all selected again. Single lines can then be changed in the lists.",
    transportHintDir: "A button hides all lines of that mode; pressed again, the previous assignment applies again.",
    sectionDefault1: "Centre", sectionDefault2: "Outbound",
    station: "Station",
    stationNone: "No station chosen yet.",
    choose: "Choose",
    change: "Change",
    search: "Search",
    searchPlaceholder: "Name of the station",
    searching: "Searching …",
    noHits: "Nothing found.",
    searchFailed: "The search did not work. Is the display connected to the internet?",
    displayName: "Display name",
    displayNamePlaceholder: "as named by MVG",
    globalId: "Global ID",
    globalIdHint: "For stations the search does not find: the MVG global ID, e.g. de:09162:10.",
    globalIdApply: "Apply",
    globalIdBad: "Format: two lower-case letters, then parts separated by colons, e.g. de:09162:10.",
    globalIdChecking: "Checking the station …",
    globalIdUnknown: "The MVG has no station with this ID (or the display is not connected to the internet).",
    globalIdSet: "Set global ID",
    noLinesChosen: "No line chosen yet.",
    firstSectionTitle: "Title: the display name above.",
    layout: "Layout",
    modeList: "One list",
    modeDir: "Two directions",
    modeTwo: "Two stations",
    section: "Section",
    linesAndDirections: "Lines and directions",
    dirHint: "Choose for each direction in which section it appears. The destinations are examples from the departures of the next hours.",
    listHint: "Tick the directions to show. With all ticked, lines added later appear too.",
    twoHint: "Each section shows one station. With all ticked, lines added later appear too.",
    secondStation: "Second station",
    sectionName: "Heading",
    hidden: "hide",
    direction: "Direction",
    noDestinations: "no departures in the next 12 hours",
    inactiveLines: "No departures in the next 12 hours",
    loadLines: "Load lines",
    loadingLines: "Loading the lines (takes a few seconds) …",
    linesFailed: "The lines could not be loaded.",
    retry: "Retry",
    typeS: "S-Bahn", typeU: "U-Bahn", typeT: "Tram", typeB: "Bus", typeR: "Regional bus", typeZ: "Regional train",
    timeFormat: "Departure time as",
    mvgIncidents: "Show disruption messages under the station",
    clock: "Clock time",
    minutes: "Minutes",
    mvgUnofficial: "Unofficial MVG interface; it may change at any time.",
    weather: "Weather",
    place: "Place",
    placeNone: "No place chosen yet.",
    placePlaceholder: "Name of the place",
    latitude: "Latitude",
    longitude: "Longitude",
    calendar: "Calendar",
    calendarSet: "address set",
    calendarUnset: "no address",
    calendarRemoving: "removed when saving",
    calendarReplacing: "replaced when saving",
    calendarHint: "Private iCal address, e.g. from Google Calendar (“Secret address in iCal format”). It is never shown and cannot be read back, only replaced or removed.",
    calendarNew: "New address",
    calendarRemove: "Remove address",
    calShow: "Display",
    calDays: "Period",
    calDaysOne: "Today",
    calDaysTwo: "Today and tomorrow",
    calDaysThree: "Next 3 days",
    calDaysWeek: "Next 7 days",
    calAllDay: "Show all-day and multi-day events",
    calPast: "Show past events of today until midnight",
    calLocation: "Show the place below the title (today and the next day)",
    calMore: "Show “+ 2 more events” if not all events fit",
    calWrap: "Long titles in two lines",
    calSpaceHint: "Places and second lines are only added where there is room; the next day may give way for them.",
    calFilter: "Filter",
    calExclude: "Hide events with these words in the title",
    calExcludeHint: "Separated by commas, e.g. “Birthday, Bins”. Upper and lower case do not matter, part of the title is enough.",
    calExcludeTooMany: "At most {n} words.",
    calFetch: "Updates",
    calRefresh: "Fetch the calendar every",
    calRefreshHint: "Changes in the calendar appear with the next fetch. Every fetch loads the whole calendar file and holds the display for a few seconds.",
    display: "Display",
    columns: "Columns from left to right",
    wideColumn: "Wide column",
    column: "Column",
    widgetMvg: "Departures",
    widgetWeather: "Weather",
    widgetCalendar: "Calendar",
    language: "Language",
    fullRefresh: "Full refresh every",
    fullRefreshHint: "Full refresh: the whole picture flashes once and removes ghosting.",
    tryOut: "To try out",
    gridEqual: "Three columns of the same width",
    dividers: "Dotted lines between the columns",
    wifi: "Wi-Fi",
    wifiConnected: "Connected to",
    signal: "Signal",
    address: "Address",
    wifiChange: "Change Wi-Fi",
    wifiScan: "Find networks",
    wifiScanning: "Searching …",
    wifiName: "Name of the Wi-Fi",
    wifiPassword: "Password",
    wifiHow: "How it works: the display leaves the current Wi-Fi and tries the new one. If that works, it saves the new Wi-Fi and restarts; the page is then reachable at the new address shown on the display. Otherwise it goes back to the old Wi-Fi. This page loses the connection for a moment.",
    wifiTest: "Test new Wi-Fi",
    wifiTesting: "Testing the new Wi-Fi …",
    wifiFailed: "The new Wi-Fi did not work (wrong password?). The display is connected to the old Wi-Fi again.",
    wifiOk: "Connected. The display restarts; the new address then appears on the display.",
    wifiNameMissing: "Please enter the name of the Wi-Fi.",
    good: "good", fair: "fair", weak: "weak",
    device: "Device",
    firmware: "Firmware",
    uptime: "Running for",
    memory: "Free memory",
    backup: "Backup",
    backupHint: "The settings as the file config.json. The calendar address and the Wi-Fi password are never in it.",
    download: "Download",
    upload: "Load file …",
    uploadLoaded: "File loaded. Please check it, then save.",
    uploadBad: "The file is not a valid config.json.",
    firmwareUpdate: "Update firmware",
    updateHint: "Asks GitHub for the newest published version – only when you press the button. GitHub sees the address (IP) of your internet connection, nothing else.",
    updateCheck: "Check for updates",
    updateChecking: "Checking …",
    updateAvailable: "Version {v} is available (installed: {i}).",
    updateUpToDate: "The firmware is up to date ({i}).",
    updateNewerInstalled: "No update: {i} is installed, a development build newer than the last version {v}.",
    updateNoRelease: "There is no published version yet.",
    updateNotFound: "No versions found – the project is not public (yet).",
    updateNoConnection: "GitHub cannot be reached. Does the display have internet?",
    updateUnexpected: "Unexpected answer from GitHub ({s}). Please try again later.",
    updateInstall: "Install",
    updateNotes: "What's new?",
    updateDownloading: "Downloading: {p} – do not switch off the display.",
    firmwareHint: "Or upload a file firmware.bin (from a release or from .pio\\build\\xiao_esp32s3_plus\\). Either way the display checks that it is an InkTavla firmware and then restarts. If the new version does not start properly, it goes back to the previous one.",
    firmwareChoose: "Choose firmware file …",
    firmwareUploading: "Uploading: {p} % – do not switch off the display.",
    firmwareFailed: "Update failed: {e}. The previous firmware stays.",
    firmwareNotBin: "Please choose the file firmware.bin.",
    advanced: "Advanced",
    timeZone: "Time zone",
    timeZoneHint: "Own time zone as a POSIX string, e.g. CET-1CEST,M3.5.0,M10.5.0/3.",
    tzCustom: "Own (POSIX string)",
    ntpServer: "Time server",
    restart: "Restart",
    reset: "Reset settings",
    resetConfirm: "Reset all settings to their defaults and delete the calendar address? Wi-Fi stays. The display restarts afterwards.",
    resetYes: "Reset",
    cancel: "Cancel",
    save: "Save and restart",
    saved: "Saved.",
    unsaved: "Unsaved changes",
    noChanges: "No changes",
    saving: "Saving …",
    saveFailed: "Saving failed",
    fixFirst: "Please correct the marked entries first.",
    issuesTitle: "The display found problems",
    issuesHint: "Nothing was saved. With “Save anyway” these entries use their defaults.",
    issuesBlocking: "Nothing was saved. Please correct the entries.",
    saveAnyway: "Save anyway",
    restarting: "The display restarts. The page reloads by itself in a moment …",
    restartSlow: "This takes longer than usual. If the page does not come back: press the button on the display.",
    connectionLost: "No connection to the display.",
    required: "Please fill in.",
    tooLong: "Too long (at most {n} characters).",
    tooManyLines: "At most 16 lines per section (lines with both directions count once).",
    noLinesInSection: "Section “{name}” has no line yet.",
    stationMissing: "Please choose a station.",
    secondStationMissing: "Please choose the second station.",
    coordinatesBad: "Latitude −90 to 90, longitude −180 to 180; give both or leave both empty.",
    urlBad: "The address must start with https:// or webcal://.",
    days: "days", hours: "h", minutesShort: "min",
    pathFile: "File", pathMvg: "Departures", pathWeather: "Weather", pathCalendar: "Calendar",
    pathLayout: "Display", pathLanguage: "Language", pathTimeZone: "Time zone", pathNtp: "Time server",
    pathCalendarUrl: "Calendar address",
    tabSystem: "System", system: "System", sysSources: "Data sources", sysLog: "Log",
    sysLogHint: "Newest entries first. It holds fixed texts and numbers only – no addresses, Wi-Fi names or calendar titles – and can be shared to get help. Warnings and errors survive a restart; after a power cut it starts again. On the display, KEY2 shows the same.",
    sysAll: "All", sysWarn: "Warnings + errors", sysErrors: "Errors only", sysRefresh: "Refresh",
    sysDownload: "Download (.txt)", sysFirmware: "Firmware", sysUptime: "Uptime",
    sysRestart: "Last restart", sysUnplanned: "unplanned", sysStart: "start {n} since power on",
    sysWifi: "Wi-Fi", sysConnected: "connected, {rssi} dBm ({q})", sysNotConnected: "not connected",
    sysClock: "Clock", sysClockSet: "set (NTP)", sysClockNotSet: "not set yet",
    sysMemory: "Memory", sysMemoryText: "{free} KB free, lowest {min} KB, largest block {block} KB",
    sysPsram: "PSRAM", sysPsramText: "{free} of {size} MB free", sysCounts: "Log",
    sysCountsText: "{w} warnings, {e} errors", qExcellent: "excellent", qGood: "good", qFair: "fair", qWeak: "weak",
    stateOk: "up to date", stateStale: "outdated", stateWaiting: "waiting", stateNoData: "no data", stateNotSetUp: "not set up",
    sysLastOk: "last {time} ({s} s)", sysFailing: "failing since {time}: {problem} ({n}×)", sysLastFailure: "last failure {time}: {problem}",
    sysTotals: "{ok} ok, {failed} failed since the start", sysNoEntries: "No entries.", sysLoadFailed: "Could not be loaded.",
  },
};

const TIME_ZONES = [
  ["CET-1CEST,M3.5.0,M10.5.0/3", { de: "Mitteleuropa (Berlin, Wien, Zürich)", en: "Central Europe (Berlin, Vienna, Zurich)" }],
  ["GMT0BST,M3.5.0/1,M10.5.0", { de: "Großbritannien (London)", en: "United Kingdom (London)" }],
  ["WET0WEST,M3.5.0/1,M10.5.0", { de: "Westeuropa (Lissabon)", en: "Western Europe (Lisbon)" }],
  ["EET-2EEST,M3.5.0/3,M10.5.0/4", { de: "Osteuropa (Helsinki, Athen)", en: "Eastern Europe (Helsinki, Athens)" }],
  ["EST5EDT,M3.2.0,M11.1.0", { de: "USA Ostküste (New York)", en: "US Eastern (New York)" }],
  ["CST6CDT,M3.2.0,M11.1.0", { de: "USA Mitte (Chicago)", en: "US Central (Chicago)" }],
  ["MST7MDT,M3.2.0,M11.1.0", { de: "USA Rocky Mountains (Denver)", en: "US Mountain (Denver)" }],
  ["PST8PDT,M3.2.0,M11.1.0", { de: "USA Westküste (Los Angeles)", en: "US Pacific (Los Angeles)" }],
];
const DEFAULT_TZ = TIME_ZONES[0][0];
const REFRESH_CHOICES = [15, 30, 60, 120, 180, 360, 720, 1440];
const CAL_REFRESH_CHOICES = [5, 10, 15, 30, 60, 120, 180, 360];
const CAL_DAYS = [1, 2, 3, 7], CAL_MAX_WORDS = 10, CAL_MAX_WORD = 32;
const TYPE_ORDER = "SUTBRZ";
const MAX_LINES = 16;

// --- Small helpers ----------------------------------------------------------

const $ = (id) => document.getElementById(id);
let lang = "de";
const t = (key, vars) => {
  let s = (TEXTS[lang] && TEXTS[lang][key]) || TEXTS.de[key] || key;
  if (vars) for (const k of Object.keys(vars)) s = s.replace("{" + k + "}", vars[k]);
  return s;
};
const bytes = (s) => new TextEncoder().encode(s).length;
const clone = (o) => JSON.parse(JSON.stringify(o));
function el(tag, attrs, ...children) {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) {
    if (v === undefined || v === null || v === false) continue;
    if (k === "class") e.className = v;
    else if (k.startsWith("on")) e.addEventListener(k.slice(2), v);
    else if (k === "text") e.textContent = v;
    else e.setAttribute(k, v === true ? "" : v);
  }
  for (const c of children.flat()) {
    if (c === null || c === undefined || c === false) continue;
    e.append(c instanceof Node ? c : document.createTextNode(String(c)));
  }
  return e;
}
function applyTexts() {
  document.documentElement.lang = lang;
  document.querySelectorAll("[data-t]").forEach((e) => { e.textContent = t(e.dataset.t); });
}
function show(id) {
  for (const v of ["loading", "login", "closed", "restarting", "main"]) $(v).hidden = v !== id;
  $("bar").hidden = id !== "main";
}
function segSet(seg, value) {
  seg.querySelectorAll("button").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.v === value)));
}
function segValue(seg) {
  const b = seg.querySelector("button[aria-pressed=true]");
  return b ? b.dataset.v : "";
}
function hhmm(date) {
  return date.toLocaleTimeString(lang === "en" ? "en-GB" : "de-DE", { hour: "2-digit", minute: "2-digit" });
}

// --- Talking to the display -----------------------------------------------------

class HttpError extends Error {
  constructor(status, body) { super("HTTP " + status); this.status = status; this.body = body; }
}
async function api(path, options) {
  const o = options || {};
  const init = { method: o.method || "GET", headers: {}, credentials: "same-origin" };
  if (o.json !== undefined) {
    init.headers["Content-Type"] = "application/json";
    init.body = JSON.stringify(o.json);
  }
  const r = await fetch(path, init);
  const text = await r.text();
  let body = null;
  try { body = text ? JSON.parse(text) : null; } catch (e) { body = text; }
  if (r.status === 401) { pageClosed(body); throw new HttpError(401, body); }
  if (!r.ok) throw new HttpError(r.status, body);
  if (!o.passive) touched();
  return body;
}

// --- State ----------------------------------------------------------------------

let S = null;          // state from the display
let config = {};       // configuration as loaded (other settings are kept as they are)
let calendarChange;    // undefined: unchanged, null: remove
let closesAt = 0;      // ms (local clock)
let dirty = false;
let saving = false;
const mvg = {          // the departures form
  station: null,       // {id, name, place}
  station2: null,
  mode: "list",
  catalogs: {},        // station id -> {state: "loading"|"ok"|"failed", lines: [...]}
  // Assignment per station: key -> {type, H: value, R: value}
  // list/two: true = shown; dir: 0 / 1 = section, -1 = hidden
  pick: {},
  pick2: {},
  names: ["", ""],
  // Per station (0: first, 1: second): modes of transport switched off,
  // saved assignments of switched-off modes (dir), set-up still to do once
  // the lines are loaded, and whether everything is to be ticked then.
  st: [],
};

function resetStationState(i, fillAll) {
  mvg.st[i] = { off: new Set(), saved: {}, pending: true, fillAll };
}

function touched() {
  if (S) closesAt = Date.now() + S.timeout_s * 1000;
  showOpenUntil();
}
function showOpenUntil() {
  $("openUntil").textContent = closesAt ? t("openUntil") + " " + hhmm(new Date(closesAt)) : "";
}
function setDirty(on) {
  dirty = on;
  const bar = $("bar");
  bar.className = "bar" + (on ? " dirty" : "");
  $("barText").textContent = on ? t("unsaved") : t("noChanges");
  $("save").disabled = !on || saving;
}
function pageClosed() {
  if ($("restarting").hidden === false) return;
  if (S && !$("main").hidden) { show("closed"); return; }
}

// --- Start ------------------------------------------------------------------------

async function start() {
  applyTexts();
  const params = new URLSearchParams(location.search);
  const code = params.get("code");
  if (code) {
    history.replaceState(null, "", location.pathname);
    try { await api("api/login", { method: "POST", json: { code } }); } catch (e) { /* login form */ }
  }
  await loadState();
}

async function loadState() {
  let r;
  try {
    r = await fetch("api/state", { credentials: "same-origin" });
  } catch (e) {
    show("closed");
    $("closed").querySelector(".hint").textContent = t("connectionLost");
    return;
  }
  const body = await r.json().catch(() => ({}));
  if (r.status === 401) {
    lang = body.lang === "en" ? "en" : "de";
    applyTexts();
    show(body.open ? "login" : "closed");
    if (body.open) $("code").focus();
    return;
  }
  S = body;
  lang = S.lang === "en" ? "en" : "de";
  applyTexts();
  closesAt = Date.now() + S.closes_in_s * 1000;
  config = S.config && typeof S.config === "object" ? S.config : {};
  fillForm();
  selectTab(initialTab());
  show("main");
  setDirty(false);
  showOpenUntil();
}

$("login").addEventListener("submit", async (e) => {
  e.preventDefault();
  $("loginError").hidden = true;
  try {
    await api("api/login", { method: "POST", json: { code: $("code").value } });
    await loadState();
  } catch (err) {
    if (err.status === 403) {
      $("loginError").textContent = t("loginWrong");
      $("loginError").hidden = false;
      $("code").select();
    } else {
      show("closed");
    }
  }
});

$("closeButton").addEventListener("click", async () => {
  try { await api("api/close", { method: "POST", passive: true }); } catch (e) { /* closed anyway */ }
  S = null;
  show("closed");
});

// Checks once a minute whether the page is still open (does not count as
// an action).
setInterval(async () => {
  if (!S || $("main").hidden) return;
  try {
    const r = await api("api/ping", { passive: true });
    closesAt = Date.now() + r.closes_in_s * 1000;
    showOpenUntil();
  } catch (e) { /* 401 shows "closed" */ }
}, 60000);

// --- Filling the forms ------------------------------------------------------------

const sec = (name) => (config.widgets && config.widgets[name]) || {};

function fillForm() {
  fillMvg();
  fillWeather();
  fillCalendar();
  fillDisplay();
  fillWifi();
  fillDevice();
  $("issues").hidden = true;
}

// Line entries "S3:H:S" -> pick map.
function parseLines(list, value) {
  const pick = {};
  for (const entry of list || []) {
    const p = String(entry).split(":");
    if (p.length !== 3) continue;
    const key = p[0].replace(/\s/g, "").toUpperCase();
    const item = pick[key] || (pick[key] = { type: p[2].toUpperCase(), label: key });
    const d = p[1].toUpperCase();
    if (d === "H" || d === "B") item.H = value;
    if (d === "R" || d === "B") item.R = value;
  }
  return pick;
}

function fillMvg() {
  const c = sec("mvg");
  const sections = Array.isArray(c.sections) && c.sections.length === 2 ? c.sections : null;
  mvg.station = c.station ? { id: c.station, name: c.name || c.station, place: "" } : null;
  mvg.station2 = null;
  mvg.pick = {};
  mvg.pick2 = {};
  mvg.names = ["", ""];
  let displayName = c.name || "";
  if (sections && sections.some((s) => s.station)) {
    mvg.mode = "two";
    const s1 = sections[0], s2 = sections[1];
    if (s1.station) mvg.station = { id: s1.station, name: s1.name || s1.station, place: "" };
    if (s2.station) mvg.station2 = { id: s2.station, name: s2.name || s2.station, place: "" };
    mvg.pick = parseLines(s1.lines, true);
    mvg.pick2 = parseLines(s2.lines, true);
    mvg.names = [s1.name || "", s2.name || ""];
    // The title of the first station is the display name (formerly a field
    // of its own: an older setting is kept).
    if (s1.name) displayName = s1.name;
  } else if (sections) {
    mvg.mode = "dir";
    mvg.names = [sections[0].name || "", sections[1].name || ""];
    mvg.pick = parseLines(sections[0].lines, 0);
    const second = parseLines(sections[1].lines, 1);
    for (const [k, v] of Object.entries(second)) {
      const item = mvg.pick[k] || (mvg.pick[k] = { type: v.type, label: v.label });
      if (v.H !== undefined) item.H = 1;
      if (v.R !== undefined) item.R = 1;
    }
  } else {
    mvg.mode = "list";
    mvg.pick = parseLines(c.lines, true);
    if (c.direction === "H" || c.direction === "R") {
      // Old setting "only direction H/R" without lines: shown in the table
      // once the lines are loaded.
      mvg.directionOnly = c.direction;
    }
  }
  // No saved line list = all lines (list and two stations).
  if (mvg.mode === "two") {
    resetStationState(0, !Object.keys(mvg.pick).length);
    resetStationState(1, !Object.keys(mvg.pick2).length);
  } else {
    resetStationState(0, mvg.mode === "list" && !Object.keys(mvg.pick).length);
    resetStationState(1, false);
  }
  $("mvgName").value = displayName;
  $("mvgName").placeholder = t("displayNamePlaceholder");
  segSet($("mvgMode"), mvg.mode);
  segSet($("mvgTime"), c.time_format === "minutes" ? "minutes" : "clock");
  $("mvgIncidents").checked = c.incidents !== false;
  showMvgStation();
  renderMvgBody();
}

// The station of the widget: chosen card or search.
function showMvgStation() {
  renderStation($("mvgStation"), () => mvg.station, (s) => {
    mvg.station = s;
    mvg.pick = {};
    resetStationState(0, true);
    // The display name follows the station; it stays editable.
    $("mvgName").value = s.name;
    renderMvgBody();
  });
}

// Station by its global ID: format check here, the name from the MVG through
// the display (also confirms that the station exists).
const STATION_ID = /^[a-z]{2}(:[A-Za-z0-9_]+)+$/;

// Station: chosen card or search. Below the search a link opens a field for
// the global ID (for stations the search does not find).
function renderStation(box, get, set) {
  box.replaceChildren();
  const current = get();
  const choose = (s) => { set(s); setDirty(true); renderStation(box, get, set); };
  const results = el("div");
  const input = el("input", { type: "text", placeholder: t("searchPlaceholder"), "aria-label": t("station") });
  const idInput = el("input", { type: "text", autocapitalize: "off", autocomplete: "off", spellcheck: "false",
    placeholder: "de:09162:10", "aria-label": t("globalId") });
  const idStatus = el("p", { hidden: true });
  const idForm = el("div", { class: "globalid", hidden: true },
    el("label", {}, t("globalId")),
    el("div", { class: "row" }, idInput,
      el("button", { type: "button", style: "flex:0 0 auto", onclick: () => applyId() }, t("globalIdApply"))),
    el("p", { class: "hint" }, t("globalIdHint")),
    idStatus);
  const idLink = el("button", { type: "button", class: "quiet small",
    onclick: () => { idForm.hidden = false; idLink.hidden = true; idInput.focus(); } }, t("globalIdSet"));
  const form = el("div", { hidden: !!current },
    el("div", { class: "row" }, input,
      el("button", { type: "button", style: "flex:0 0 auto", onclick: () => run() }, t("search"))),
    results, idLink, idForm);
  input.addEventListener("keydown", (e) => { if (e.key === "Enter") { e.preventDefault(); run(); } });
  idInput.addEventListener("keydown", (e) => { if (e.key === "Enter") { e.preventDefault(); applyId(); } });
  async function run() {
    const q = input.value.trim();
    if (q.length < 2) return;
    results.replaceChildren(el("p", { class: "hint" }, el("span", { class: "spin" }), t("searching")));
    try {
      const hits = await api("api/mvg/stations?q=" + encodeURIComponent(q));
      if (!hits.length) { results.replaceChildren(el("p", { class: "hint" }, t("noHits"))); return; }
      results.replaceChildren(el("div", { class: "hits" }, hits.map((h) =>
        el("button", { type: "button", onclick: () => choose({ id: h.id, name: h.name, place: h.place }) },
          el("b", {}, h.name), " ", el("small", {}, [h.place, typesText(h.types)].filter(Boolean).join(" · "))))));
    } catch (e) {
      if (e.status !== 401) results.replaceChildren(el("p", { class: "err" }, t("searchFailed")));
    }
  }
  async function applyId() {
    const id = idInput.value.trim();
    const say = (cls, ...content) => { idStatus.className = cls; idStatus.replaceChildren(...content); idStatus.hidden = false; };
    if (!STATION_ID.test(id) || id.length > 32) { say("err", t("globalIdBad")); return; }
    say("hint", el("span", { class: "spin" }), t("globalIdChecking"));
    try {
      const found = await api("api/mvg/station?id=" + encodeURIComponent(id));
      choose({ id: found.id, name: found.name, place: "" });
    } catch (e) {
      if (e.status !== 401) say("err", t("globalIdUnknown"));
    }
  }
  if (current) {
    box.append(el("div", { class: "chosen" },
      el("div", {}, el("b", {}, current.name), el("small", {}, [current.place, current.id].filter(Boolean).join(" · "))),
      el("button", { type: "button", class: "quiet", onclick: () => { form.hidden = !form.hidden; if (!form.hidden) input.focus(); } }, t("change"))));
  } else {
    box.append(el("div", { class: "empty" }, t("stationNone")));
  }
  box.append(form);
  if (!current) form.hidden = false;
}

function typesText(types) {
  return (types || "").split("").map((c) => t("type" + c)).join(", ");
}

// Lines of a station (cached; loading takes several requests on the
// display).
async function loadCatalog(id, rerender) {
  if (!id) return;
  const c = mvg.catalogs[id];
  if (c && (c.state === "ok" || c.state === "loading")) return;
  // No rerender here: the caller draws the "loading" state itself (a nested
  // rerender inside renderMvgBody() drew status and table twice).
  mvg.catalogs[id] = { state: "loading", lines: [] };
  try {
    const lines = await api("api/mvg/lines?station=" + encodeURIComponent(id));
    mvg.catalogs[id] = { state: "ok", lines };
  } catch (e) {
    mvg.catalogs[id] = { state: "failed", lines: [] };
  }
  rerender();
}

// Lines to show: those of the catalog and those already selected.
function rowsFor(id, pick) {
  const cat = mvg.catalogs[id];
  const lines = cat && cat.state === "ok" ? cat.lines.map((l) => ({ ...l })) : [];
  for (const [key, v] of Object.entries(pick)) {
    if (!lines.some((l) => l.key === key)) lines.push({ key, label: v.label || key, type: v.type, H: [], R: [] });
  }
  lines.sort((a, b) => TYPE_ORDER.indexOf(a.type) - TYPE_ORDER.indexOf(b.type));
  return lines;
}

// Directions listed for a line: those with departures in the next 12 hours
// or already selected; a line without any departures lists both (in the
// closed group at the end).
const isInactive = (line) => !["H", "R"].some((d) => (line[d] || []).length);
function dirsOf(line, pick) {
  if (isInactive(line)) return ["H", "R"];
  return ["H", "R"].filter((d) => (line[d] || []).length || (!!pick[line.key] && pick[line.key][d] !== undefined));
}

// Modes of transport that run at a station (in display order).
function modesOf(rows) {
  return [...TYPE_ORDER].filter((type) => rows.some((l) => l.type === type));
}

function catalogReady(id) {
  const cat = mvg.catalogs[id];
  return !!cat && cat.state === "ok";
}

// Sets up the mode buttons of a station once its lines are known. A new
// station or one without a saved line list starts with everything selected;
// a saved selection switches off the modes without a selected line.
function initStationState(i, id, pick) {
  const st = mvg.st[i];
  if (!st.pending || !catalogReady(id)) return;
  st.pending = false;
  const rows = rowsFor(id, pick);
  st.off = new Set();
  if (mvg.mode === "dir") {
    for (const type of modesOf(rows)) {
      if (Object.keys(pick).length && !rows.some((l) => l.type === type && pick[l.key])) st.off.add(type);
    }
    return;
  }
  if (st.fillAll) {
    for (const line of rows) setLine(pick, line, true);
    st.fillAll = false;
    return;
  }
  for (const type of modesOf(rows)) {
    if (!rows.some((l) => l.type === type && pick[l.key])) st.off.add(type);
  }
}

// list/two: ticks every listed direction of a line (on) or clears it.
function setLine(pick, line, on) {
  if (!on) { delete pick[line.key]; return; }
  const it = pick[line.key] || (pick[line.key] = { type: line.type, label: line.label });
  for (const d of dirsOf(line, pick)) it[d] = true;
}

// list/two: everything ticked = no line list is saved (all lines, also
// future ones). Before the lines are loaded, a station without a saved list
// counts as "all".
function allTicked(i, id, pick) {
  const st = mvg.st[i];
  if (st.pending && st.fillAll) return true;
  // Lines not loaded (failed): the saved selection stays as it is.
  if (!catalogReady(id)) return !Object.keys(pick).length;
  const rows = rowsFor(id, pick);
  return rows.every((line) => dirsOf(line, pick).every((d) => !!pick[line.key] && pick[line.key][d] === true));
}

// One button per mode of transport at this station. list/two: pressing an
// active mode clears all its lines and hides its list; pressing it again
// shows the list with all its lines ticked. dir: pressing an active mode
// hides all its lines and closes the list; pressing it again restores the
// previous assignment.
function modeButtons(i, id, pick, rows) {
  const st = mvg.st[i];
  const modes = modesOf(rows);
  if (!modes.length) return null;
  const toggle = (type) => {
    const mine = rows.filter((l) => l.type === type);
    if (!st.off.has(type)) {
      st.off.add(type);
      if (mvg.mode === "dir") {
        const keep = {};
        for (const line of mine) if (pick[line.key]) keep[line.key] = { ...pick[line.key] };
        st.saved[type] = keep;
      }
      for (const line of mine) delete pick[line.key];
    } else {
      st.off.delete(type);
      if (mvg.mode === "dir") {
        for (const [key, v] of Object.entries(st.saved[type] || {})) pick[key] = v;
        delete st.saved[type];
      } else {
        for (const line of mine) setLine(pick, line, true);
      }
    }
    setDirty(true);
    renderMvgBody();
  };
  return el("div", {},
    el("span", { class: "label" }, t("transportModes")),
    el("div", { class: "chips" }, modes.map((type) => el("button", {
      type: "button", class: "chip", "aria-pressed": String(!st.off.has(type)), onclick: () => toggle(type),
    }, t("type" + type)))),
    el("p", { class: "hint" }, t(mvg.mode === "dir" ? "transportHintDir" : "transportHint")));
}

// Departures part of one station: status, mode buttons and the table.
function stationLines(i, id, pick, control, rerender) {
  const box = el("div");
  loadCatalog(id, rerender);
  initStationState(i, id, pick);
  const status = catalogStatus(id, rerender);
  if (status) box.append(status);
  const rows = rowsFor(id, pick);
  if (!mvg.st[i].pending) {
    const buttons = modeButtons(i, id, pick, rows);
    if (buttons) box.append(buttons);
  }
  box.append(linesTable(i, rows, pick, control));
  return box;
}

function catalogStatus(id, rerender) {
  const cat = mvg.catalogs[id];
  if (!cat || cat.state === "loading") {
    return el("p", { class: "hint" }, el("span", { class: "spin" }), t("loadingLines"));
  }
  if (cat.state === "failed") {
    return el("p", { class: "err" }, t("linesFailed"), " ",
      el("button", { type: "button", class: "quiet", onclick: () => { delete mvg.catalogs[id]; loadCatalog(id, rerender); rerender(); } }, t("retry")));
  }
  return null;
}

function sym(line) {
  return el("span", { class: "sym " + line.type }, line.label.replace(/\s/g, ""));
}

// Table of lines and directions, one open list per mode of transport that
// is switched on; lines without any departures go into a closed group at
// the end. control(line, dir) returns the input.
function linesTable(i, rows, pick, control) {
  const off = mvg.st[i].off;
  const box = el("div", { class: "lines" });
  const row = (line, dir) => {
    const dest = (line[dir] || []).join(", ");
    return el("div", { class: "line" }, sym(line),
      el("div", { class: "dest" }, dest || el("i", {}, t("noDestinations")), el("small", {}, t("direction") + " " + dir)),
      control(line, dir));
  };
  const inactive = [];
  let group = null;
  let details = null;
  for (const line of rows) {
    if (off.has(line.type)) continue;
    if (isInactive(line)) { inactive.push(line); continue; }
    if (line.type !== group) {
      group = line.type;
      details = el("details", { open: true }, el("summary", { class: "grp" }, t("type" + group)));
      box.append(details);
    }
    for (const dir of dirsOf(line, pick)) details.append(row(line, dir));
  }
  if (inactive.length) {
    // Open when only some of these lines are chosen (so the choice is seen).
    const ticked = inactive.filter((l) => pick[l.key]).length;
    const rest = el("details", { open: (ticked && ticked < inactive.length) || undefined }, el("summary", { class: "grp" }, t("inactiveLines") + " (" + inactive.length + ")"));
    for (const line of inactive) for (const dir of ["H", "R"]) rest.append(row(line, dir));
    box.append(rest);
  }
  return box;
}

function renderMvgBody() {
  const body = $("mvgModeBody");
  body.replaceChildren();
  const rerender = () => renderMvgBody();
  const id = mvg.station && mvg.station.id;
  if (mvg.mode === "list") {
    body.append(el("h3", {}, t("linesAndDirections")), el("p", { class: "hint" }, t("listHint")));
    if (!id) return;
    body.append(stationLines(0, id, mvg.pick, (line, dir) => checkbox(mvg.pick, line, dir), rerender));
    body.append(el("p", { class: "err", id: "mvgLinesError", hidden: true }));
  } else if (mvg.mode === "dir") {
    body.append(el("div", { class: "pair" },
      [0, 1].map((i) => el("div", {},
        el("label", { for: "secName" + i }, t("section") + " " + (i + 1)),
        el("input", { id: "secName" + i, type: "text", maxlength: 24, value: mvg.names[i],
          placeholder: t(i === 0 ? "sectionDefault1" : "sectionDefault2"),
          oninput: (e) => { mvg.names[i] = e.target.value; setDirty(true); } })))));
    body.append(el("h3", {}, t("linesAndDirections")), el("p", { class: "hint" }, t("dirHint")));
    if (!id) return;
    body.append(stationLines(0, id, mvg.pick, (line, dir) => sectionSelect(line, dir), rerender));
    body.append(el("p", { class: "err", id: "mvgLinesError", hidden: true }));
  } else {
    body.append(el("p", { class: "hint" }, t("twoHint")));
    [0, 1].forEach((i) => {
      const station = i === 0 ? mvg.station : mvg.station2;
      const pick = i === 0 ? mvg.pick : mvg.pick2;
      body.append(el("h3", {}, t("section") + " " + (i + 1)));
      if (i === 0) {
        body.append(el("p", { class: "hint" }, t("station") + ": " + (station ? station.name : "–") +
          ". " + t("firstSectionTitle")));
      }
      if (i === 1) {
        const box = el("div");
        body.append(el("span", { class: "label" }, t("secondStation")), box);
        renderStation(box, () => mvg.station2, (s) => {
          mvg.station2 = s;
          mvg.pick2 = {};
          resetStationState(1, true);
          renderMvgBody();
        });
        body.append(el("label", { for: "secName1" }, t("sectionName")),
          el("input", { id: "secName1", type: "text", maxlength: 24, value: mvg.names[1], placeholder: station ? station.name : "",
            oninput: (e) => { mvg.names[1] = e.target.value; setDirty(true); } }));
      }
      if (station) body.append(stationLines(i, station.id, pick, (line, dir) => checkbox(pick, line, dir), rerender));
    });
    body.append(el("p", { class: "err", id: "mvgLinesError", hidden: true }));
  }
}

function checkbox(pick, line, dir) {
  const item = pick[line.key];
  return el("input", { type: "checkbox", "aria-label": line.label + " " + dir, checked: !!(item && item[dir]),
    onchange: (e) => {
      const it = pick[line.key] || (pick[line.key] = { type: line.type, label: line.label });
      if (e.target.checked) it[dir] = true; else delete it[dir];
      if (it.H === undefined && it.R === undefined) delete pick[line.key];
      setDirty(true);
    } });
}

function sectionSelect(line, dir) {
  const item = mvg.pick[line.key];
  const value = item && item[dir] !== undefined ? item[dir] : -1;
  const name = (i) => mvg.names[i] || t("section") + " " + (i + 1);
  return el("select", { "aria-label": line.label + " " + dir,
    onchange: (e) => {
      const v = Number(e.target.value);
      const it = mvg.pick[line.key] || (mvg.pick[line.key] = { type: line.type, label: line.label });
      if (v < 0) delete it[dir]; else it[dir] = v;
      if (it.H === undefined && it.R === undefined) delete mvg.pick[line.key];
      setDirty(true);
    } },
  el("option", { value: 0, selected: value === 0 }, name(0)),
  el("option", { value: 1, selected: value === 1 }, name(1)),
  el("option", { value: -1, selected: value === -1 }, t("hidden")));
}

$("mvgMode").addEventListener("click", (e) => {
  const b = e.target.closest("button");
  if (!b || b.dataset.v === mvg.mode) return;
  // The lines start empty in another layout (the meaning differs).
  mvg.mode = b.dataset.v;
  mvg.pick = {};
  mvg.pick2 = {};
  mvg.names = ["", ""];
  resetStationState(0, mvg.mode !== "dir");
  resetStationState(1, true);
  segSet($("mvgMode"), mvg.mode);
  renderMvgBody();
  setDirty(true);
});
$("mvgTime").addEventListener("click", (e) => {
  const b = e.target.closest("button");
  if (!b) return;
  segSet($("mvgTime"), b.dataset.v);
  setDirty(true);
});

// pick map -> line entries; a line with both directions of the same value
// becomes one entry "B".
function linesOf(pick, value) {
  const out = [];
  for (const [key, it] of Object.entries(pick)) {
    const h = it.H === value, r = it.R === value;
    if (h && r) out.push(key + ":B:" + it.type);
    else if (h) out.push(key + ":H:" + it.type);
    else if (r) out.push(key + ":R:" + it.type);
  }
  return out;
}

// --- Weather -----------------------------------------------------------------------

let place = null;  // {name, region, lat, lon}
function fillWeather() {
  const c = sec("weather");
  place = typeof c.latitude === "number" && typeof c.longitude === "number"
    ? { name: c.name || "", region: "", lat: c.latitude, lon: c.longitude } : null;
  $("weatherName").value = c.name || "";
  $("weatherLat").value = place ? String(place.lat) : "";
  $("weatherLon").value = place ? String(place.lon) : "";
  renderPlace();
}
function fmtCoord(v) { return Number(v).toLocaleString(lang === "en" ? "en-GB" : "de-DE", { maximumFractionDigits: 4 }); }
function renderPlace() {
  const box = $("weatherPlace");
  box.replaceChildren();
  const results = el("div");
  const input = el("input", { type: "text", placeholder: t("placePlaceholder"), "aria-label": t("place") });
  const form = el("div", { hidden: !!place },
    el("div", { class: "row" }, input, el("button", { type: "button", style: "flex:0 0 auto", onclick: () => run() }, t("search"))),
    results);
  input.addEventListener("keydown", (e) => { if (e.key === "Enter") { e.preventDefault(); run(); } });
  async function run() {
    const q = input.value.trim();
    if (q.length < 2) return;
    results.replaceChildren(el("p", { class: "hint" }, el("span", { class: "spin" }), t("searching")));
    try {
      const hits = await api("api/weather/places?q=" + encodeURIComponent(q) + "&lang=" + lang);
      if (!hits.length) { results.replaceChildren(el("p", { class: "hint" }, t("noHits"))); return; }
      results.replaceChildren(el("div", { class: "hits" }, hits.map((h) =>
        el("button", { type: "button", onclick: () => {
          place = h;
          $("weatherName").value = h.name;
          $("weatherLat").value = String(h.lat);
          $("weatherLon").value = String(h.lon);
          setDirty(true);
          renderPlace();
        } }, el("b", {}, h.name), " ", el("small", {}, [h.region, h.country].filter(Boolean).join(", "))))));
    } catch (e) {
      if (e.status !== 401) results.replaceChildren(el("p", { class: "err" }, t("searchFailed")));
    }
  }
  if (place) {
    box.append(el("div", { class: "chosen" },
      el("div", {}, el("b", {}, place.name || $("weatherName").value || "–"),
        el("small", {}, [place.region, fmtCoord(place.lat) + " / " + fmtCoord(place.lon)].filter(Boolean).join(" · "))),
      el("button", { type: "button", class: "quiet", onclick: () => { form.hidden = !form.hidden; if (!form.hidden) input.focus(); } }, t("change"))));
  } else {
    box.append(el("div", { class: "empty" }, t("placeNone")));
  }
  box.append(form);
}
for (const id of ["weatherLat", "weatherLon"]) {
  $(id).addEventListener("change", () => {
    const lat = parseNumber($("weatherLat").value), lon = parseNumber($("weatherLon").value);
    place = lat !== null && lon !== null ? { name: $("weatherName").value, region: "", lat, lon } : null;
    renderPlace();
  });
}
function parseNumber(text) {
  const s = String(text).trim().replace(",", ".");
  if (!s) return null;
  const v = Number(s);
  return Number.isFinite(v) ? v : NaN;
}

// --- Calendar ---------------------------------------------------------------------

function minutesText(m) {
  return m % 60 === 0 && m >= 60 ? m / 60 + " h" : m + " min";
}
function fillCalendar() {
  calendarChange = undefined;
  $("calendarUrl").value = "";
  showCalendarState();
  const c = sec("calendar");
  const days = CAL_DAYS.includes(c.days) ? c.days : 7;
  const dayNames = { 1: "calDaysOne", 2: "calDaysTwo", 3: "calDaysThree", 7: "calDaysWeek" };
  $("calDays").replaceChildren(...CAL_DAYS.map((n) =>
    el("option", { value: n, selected: n === days }, t(dayNames[n]))));
  $("calAllDay").checked = c.all_day !== false;
  $("calPast").checked = c.past !== false;
  $("calLocation").checked = c.location === true;
  $("calMore").checked = c.more === true;
  $("calWrap").checked = c.wrap !== false;
  $("calExclude").value = Array.isArray(c.exclude) ? c.exclude.filter((w) => typeof w === "string").join(", ") : "";
  const refresh = Number.isInteger(c.refresh_minutes) ? c.refresh_minutes : 15;
  const choices = CAL_REFRESH_CHOICES.includes(refresh) ? CAL_REFRESH_CHOICES : [...CAL_REFRESH_CHOICES, refresh].sort((a, b) => a - b);
  $("calRefresh").replaceChildren(...choices.map((m) =>
    el("option", { value: m, selected: m === refresh }, minutesText(m))));
}
function showCalendarState() {
  const p = $("calendarState");
  let key = S.secrets.calendar_url ? "calendarSet" : "calendarUnset";
  if (calendarChange === null) key = "calendarRemoving";
  else if (calendarChange) key = "calendarReplacing";
  p.textContent = t(key);
  p.className = "pill" + (key === "calendarSet" ? "" : " off");
  $("calendarRemove").hidden = !S.secrets.calendar_url || calendarChange === null;
}
$("calendarUrl").addEventListener("input", (e) => {
  const v = e.target.value.trim();
  calendarChange = v ? v : undefined;
  showCalendarState();
  setDirty(true);
});
$("calendarRemove").addEventListener("click", () => {
  calendarChange = null;
  $("calendarUrl").value = "";
  showCalendarState();
  setDirty(true);
});

// --- Display -----------------------------------------------------------------------

function fillDisplay() {
  const layout = config.layout || {};
  const slots = Array.isArray(layout.slots) ? layout.slots : ["mvg", "calendar", "weather"];
  const box = $("slots");
  box.replaceChildren();
  S.slots.forEach((size, i) => {
    const fitting = S.widgets.filter((w) => w.sizes.includes(size));
    const value = slots[i] || "";
    const select = el("select", { id: "slot" + i, onchange: () => setDirty(true) },
      fitting.map((w) => el("option", { value: w.id, selected: w.id === value }, widgetName(w.id))));
    if (value && !fitting.some((w) => w.id === value)) {
      select.append(el("option", { value, selected: true }, value));
    }
    box.append(el("div", {}, el("label", { class: "label", for: "slot" + i }, size === "wide" ? t("wideColumn") : t("column") + " " + (i + 1)), select));
  });
  $("language").value = config.language === "en" ? "en" : "de";
  const refresh = (config.display && config.display.full_refresh_minutes) || 360;
  const choices = REFRESH_CHOICES.includes(refresh) ? REFRESH_CHOICES : [...REFRESH_CHOICES, refresh].sort((a, b) => a - b);
  $("fullRefresh").replaceChildren(...choices.map((m) =>
    el("option", { value: m, selected: m === refresh }, minutesText(m))));
  $("gridEqual").checked = layout.grid === "equal";
  $("dividers").checked = layout.dividers !== false;  // on by default
}
function widgetName(id) {
  const key = "widget" + id.charAt(0).toUpperCase() + id.slice(1);
  return TEXTS[lang][key] ? t(key) : id;
}

// --- Wi-Fi ----------------------------------------------------------------------------

// Signal strength as four bars (drawn by CSS) and a word.
function bars(rssi) {
  const level = rssi >= -60 ? 4 : rssi >= -70 ? 3 : rssi >= -80 ? 2 : 1;
  const icon = el("span", { class: "bars", "data-level": String(level), "aria-hidden": "true" },
    el("i"), el("i"), el("i"), el("i"));
  return [icon, t(level === 4 ? "good" : level === 3 ? "fair" : "weak")];
}
function fillWifi() {
  const w = S.wifi || {};
  const [b, word] = bars(w.rssi || -100);
  $("wifiInfo").replaceChildren(
    el("dt", {}, t("wifiConnected")), el("dd", {}, w.ssid || "–"),
    el("dt", {}, t("signal")), el("dd", {}, b, " " + word + " (" + w.rssi + " dBm)"),
    el("dt", {}, t("address")), el("dd", {}, w.ip || "–"));
  $("wifiNets").replaceChildren();
  $("wifiNets").hidden = true;
  $("wifiResult").hidden = true;
}
$("wifiScan").addEventListener("click", async () => {
  const box = $("wifiNets");
  box.hidden = false;
  box.replaceChildren(el("p", { class: "hint", style: "padding:8px 10px;margin:0" }, el("span", { class: "spin" }), t("wifiScanning")));
  try {
    const nets = await api("api/wifi/scan");
    box.replaceChildren(...nets.map((n) => {
      const [b] = bars(n.rssi);
      return el("button", { type: "button", "aria-pressed": "false", onclick: (e) => {
        box.querySelectorAll("button").forEach((x) => x.setAttribute("aria-pressed", String(x === e.currentTarget)));
        $("wifiSsid").value = n.ssid;
        $("wifiPassword").focus();
      } }, n.ssid + (n.secure ? "" : " (offen)"), b);
    }));
    if (!nets.length) box.replaceChildren(el("p", { class: "hint", style: "padding:8px 10px;margin:0" }, t("noHits")));
  } catch (e) {
    if (e.status !== 401) box.replaceChildren(el("p", { class: "err", style: "padding:8px 10px;margin:0" }, t("searchFailed")));
  }
});
$("wifiTest").addEventListener("click", async () => {
  const result = $("wifiResult");
  const ssid = $("wifiSsid").value.trim();
  if (!ssid) { result.textContent = t("wifiNameMissing"); result.className = "err"; result.hidden = false; return; }
  result.className = "hint";
  result.replaceChildren(el("span", { class: "spin" }), t("wifiTesting"));
  result.hidden = false;
  $("wifiTest").disabled = true;
  try {
    await api("api/wifi", { method: "POST", json: { ssid, password: $("wifiPassword").value } });
  } catch (e) {
    $("wifiTest").disabled = false;
    if (e.status !== 401) { result.className = "err"; result.textContent = t("wifiFailed"); }
    return;
  }
  // The display leaves the network now: ask until it answers again (old
  // network) or give up (it restarted in the new network).
  const startedAt = Date.now();
  const poll = async () => {
    try {
      const r = await fetch("api/wifi/status", { credentials: "same-origin" });
      const s = await r.json();
      if (s.state === "testing") { setTimeout(poll, 2000); return; }
      $("wifiTest").disabled = false;
      result.className = s.state === "failed" ? "err" : "hint";
      result.textContent = s.state === "failed" ? t("wifiFailed") : t("wifiOk");
    } catch (e) {
      if (Date.now() - startedAt < 60000) { setTimeout(poll, 2000); return; }
      result.className = "hint";
      result.textContent = t("wifiOk");
    }
  };
  setTimeout(poll, 3000);
});

// --- Device -----------------------------------------------------------------------------

function duration(s) {
  const d = Math.floor(s / 86400), h = Math.floor((s % 86400) / 3600), m = Math.floor((s % 3600) / 60);
  if (d) return d + " " + t("days") + ", " + h + " " + t("hours");
  if (h) return h + " " + t("hours") + " " + m + " " + t("minutesShort");
  return m + " " + t("minutesShort");
}
function fillDevice() {
  const d = S.device || {};
  $("deviceInfo").replaceChildren(
    el("dt", {}, t("firmware")), el("dd", {}, d.firmware || "–"),
    el("dt", {}, t("uptime")), el("dd", {}, duration(d.uptime_s || 0)),
    el("dt", {}, t("memory")), el("dd", {}, Math.round((d.heap || 0) / 1024) + " KB intern, " +
      ((d.psram || 0) / 1048576).toLocaleString(lang === "en" ? "en-GB" : "de-DE", { maximumFractionDigits: 1 }) + " MB PSRAM"));
  const tz = config.time_zone || DEFAULT_TZ;
  const known = TIME_ZONES.some(([v]) => v === tz);
  $("timeZone").replaceChildren(
    ...TIME_ZONES.map(([v, names]) => el("option", { value: v, selected: v === tz }, names[lang])),
    el("option", { value: "", selected: !known }, t("tzCustom")));
  $("timeZoneCustom").value = known ? "" : tz;
  $("timeZoneCustom").hidden = known;
  $("timeZoneHint").hidden = known;
  $("ntpServer").value = config.ntp_server || "pool.ntp.org";
  $("uploadResult").hidden = true;
  $("firmwareResult").hidden = true;
  $("updateResult").hidden = true;
  $("updateOffer").hidden = true;
  $("resetConfirm").hidden = true;
}
$("timeZone").addEventListener("change", (e) => {
  const custom = e.target.value === "";
  $("timeZoneCustom").hidden = !custom;
  $("timeZoneHint").hidden = !custom;
  if (custom) $("timeZoneCustom").focus();
});
$("uploadButton").addEventListener("click", () => $("uploadFile").click());
$("uploadFile").addEventListener("change", async (e) => {
  const file = e.target.files[0];
  e.target.value = "";
  if (!file) return;
  const out = $("uploadResult");
  out.hidden = false;
  try {
    const parsed = JSON.parse(await file.text());
    if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) throw new Error("not an object");
    config = parsed;
    fillForm();
    setDirty(true);
    out.className = "hint";
    out.textContent = t("uploadLoaded");
  } catch (err) {
    out.className = "err";
    out.textContent = t("uploadBad");
  }
});
// Reasons from the display (core/firmware_writer.h, settings_server.cpp).
const FIRMWARE_ERRORS = {
  "not an InkTavla firmware": { de: "keine InkTavla-Firmware", en: "not an InkTavla firmware" },
  "not an ESP32-S3 firmware image": { de: "keine Firmware für den ESP32-S3", en: "not a firmware for the ESP32-S3" },
  "the image is not valid": { de: "die Datei ist beschädigt", en: "the file is damaged" },
  "the file is too large": { de: "die Datei ist zu groß", en: "the file is too large" },
  "cannot start the update": { de: "das Update ließ sich nicht starten", en: "the update could not start" },
  "cannot write to flash": { de: "Schreiben in den Speicher fehlgeschlagen", en: "writing to the flash failed" },
  "upload aborted": { de: "Übertragung abgebrochen", en: "upload aborted" },
  "an update is already running": { de: "es läuft schon ein Update", en: "an update is already running" },
  "check for updates first": { de: "bitte zuerst nach Updates suchen", en: "please check for updates first" },
  "the release has no firmware.bin": { de: "das Release enthält keine firmware.bin", en: "the release has no firmware.bin" },
  "no connection": { de: "keine Verbindung zu GitHub", en: "no connection to GitHub" },
  "download incomplete": { de: "Download unvollständig", en: "download incomplete" },
  "not enough memory": { de: "zu wenig Speicher", en: "not enough memory" },
};
function firmwareError(error) {
  return FIRMWARE_ERRORS[error] ? FIRMWARE_ERRORS[error][lang] : error;
}

// Update button: asks the display, which asks GitHub (core/update_check.h).
let offeredVersion = null;
function firmwareButtons(enabled) {
  $("updateCheck").disabled = !enabled;
  $("updateInstall").disabled = !enabled;
  $("firmwareButton").disabled = !enabled;
}
$("updateCheck").addEventListener("click", async () => {
  const out = $("updateResult");
  out.hidden = false;
  out.className = "hint";
  out.textContent = t("updateChecking");
  $("updateOffer").hidden = true;
  offeredVersion = null;
  firmwareButtons(false);
  try {
    const r = await api("api/update/check");
    const vars = { i: r.installed || "–", v: r.latest || "–", s: r.http_status || "?" };
    const texts = {
      available: "updateAvailable", up_to_date: "updateUpToDate", newer_installed: "updateNewerInstalled",
      no_release: "updateNoRelease", not_found: "updateNotFound", no_connection: "updateNoConnection",
    };
    out.textContent = t(texts[r.status] || "updateUnexpected", vars);
    out.className = r.status === "no_connection" || !texts[r.status] ? "err" : "hint";
    if (r.status === "available") {
      offeredVersion = r.latest;
      const notes = typeof r.notes_url === "string" && r.notes_url.startsWith("https://github.com/");
      $("updateNotes").hidden = !notes;
      if (notes) $("updateNotes").href = r.notes_url;
      $("updateOffer").hidden = false;
    }
  } catch (e) {
    if (e.status === 401) return;
    out.className = "err";
    let error = e.body && e.body.error;
    out.textContent = error ? t("firmwareFailed", { e: firmwareError(error) }) : t("connectionLost");
  } finally {
    firmwareButtons(true);
  }
});
$("updateInstall").addEventListener("click", async () => {
  if (!offeredVersion) return;
  const out = $("updateResult");
  out.className = "hint";
  out.textContent = t("updateDownloading", { p: "0 %" });
  firmwareButtons(false);
  const failed = (error) => {
    out.className = "err";
    out.textContent = t("firmwareFailed", { e: firmwareError(error) });
    firmwareButtons(true);
  };
  try {
    await api("api/update/install", { method: "POST", json: { version: offeredVersion } });
  } catch (e) {
    if (e.status !== 401) failed((e.body && e.body.error) || t("connectionLost"));
    return;
  }
  let misses = 0;
  const poll = async () => {
    try {
      const s = await api("api/update/status", { passive: true });
      misses = 0;
      if (s.state === "done") { waitForRestart(); return; }
      if (s.state === "failed") { failed(s.error || "?"); return; }
      const p = s.total ? Math.floor((s.bytes / s.total) * 100) + " %" : Math.floor(s.bytes / 1024) + " KB";
      out.textContent = t("updateDownloading", { p });
    } catch (e) {
      if (e.status === 401) return;
      // The display may already be restarting.
      if (++misses >= 5) { waitForRestart(); return; }
    }
    setTimeout(poll, 1000);
  };
  setTimeout(poll, 1000);
});

// Firmware: upload with progress (XMLHttpRequest reports it, fetch does not).
$("firmwareButton").addEventListener("click", () => $("firmwareFile").click());
$("firmwareFile").addEventListener("change", (e) => {
  const file = e.target.files[0];
  e.target.value = "";
  if (!file) return;
  const out = $("firmwareResult");
  out.hidden = false;
  if (!/\.bin$/i.test(file.name)) { out.className = "err"; out.textContent = t("firmwareNotBin"); return; }
  const form = new FormData();
  form.append("firmware", file, file.name);
  const xhr = new XMLHttpRequest();
  xhr.open("POST", "api/firmware");
  $("firmwareButton").disabled = true;
  out.className = "hint";
  out.textContent = t("firmwareUploading", { p: 0 });
  xhr.upload.onprogress = (p) => {
    if (p.lengthComputable) out.textContent = t("firmwareUploading", { p: Math.floor((p.loaded / p.total) * 100) });
  };
  xhr.onload = () => {
    $("firmwareButton").disabled = false;
    if (xhr.status === 200) { waitForRestart(); return; }
    if (xhr.status === 401) { pageClosed(); return; }
    let error = "HTTP " + xhr.status;
    try { error = JSON.parse(xhr.responseText).error || error; } catch (err) { /* keep */ }
    out.className = "err";
    out.textContent = t("firmwareFailed", { e: firmwareError(error) });
  };
  xhr.onerror = () => {
    $("firmwareButton").disabled = false;
    out.className = "err";
    out.textContent = t("firmwareFailed", { e: t("connectionLost") });
  };
  xhr.send(form);
});

$("restart").addEventListener("click", async () => {
  try { await api("api/restart", { method: "POST" }); waitForRestart(); } catch (e) { /* shown */ }
});
$("reset").addEventListener("click", () => { $("resetConfirm").hidden = false; });
$("resetNo").addEventListener("click", () => { $("resetConfirm").hidden = true; });
$("resetYes").addEventListener("click", async () => {
  try { await api("api/reset", { method: "POST" }); waitForRestart(); } catch (e) { /* shown */ }
});

// Any change in a form field counts.
document.querySelector("main").addEventListener("input", (e) => {
  if (e.target.closest("#wifiChange") || e.target.type === "file" || e.target.id === "code") return;
  setDirty(true);
});
document.querySelector("main").addEventListener("change", (e) => {
  if (e.target.closest("#wifiChange") || e.target.type === "file") return;
  setDirty(true);
});

// --- Building and checking the configuration -------------------------------------

function setOrDelete(obj, key, value) {
  if (value === "" || value === undefined || value === null) delete obj[key];
  else obj[key] = value;
}

// The configuration from the forms; settings without a form field stay as
// they were. Returns {config, errors: [[element id or null, message]]}.
function buildConfig() {
  const c = clone(config);
  const errors = [];
  const check = (id, max) => {
    const v = $(id).value.trim();
    if (bytes(v) > max) errors.push([id, t("tooLong", { n: max })]);
    return v;
  };
  c.version = 1;
  c.widgets = c.widgets || {};

  // Departures
  const m = c.widgets.mvg = c.widgets.mvg || {};
  delete m.lines;
  delete m.sections;
  delete m.direction;
  setOrDelete(m, "name", check("mvgName", 64));
  delete m.note;  // removed setting
  m.time_format = segValue($("mvgTime")) || "clock";
  if ($("mvgIncidents").checked) delete m.incidents; else m.incidents = false;
  const usesMvg = [0, 1, 2].some((i) => $("slot" + i) && $("slot" + i).value === "mvg");
  const lineError = (msg) => errors.push(["mvgLinesError", msg]);
  if (mvg.mode === "list") {
    setOrDelete(m, "station", mvg.station ? mvg.station.id : "");
    if (mvg.station && !allTicked(0, mvg.station.id, mvg.pick)) {
      const lines = linesOf(mvg.pick, true);
      if (lines.length > MAX_LINES) lineError(t("tooManyLines"));
      if (!lines.length) lineError(t("noLinesChosen"));
      m.lines = lines;
    }
    if (!mvg.station && usesMvg) errors.push(["mvgStation", t("stationMissing")]);
  } else if (mvg.mode === "dir") {
    setOrDelete(m, "station", mvg.station ? mvg.station.id : "");
    if (!mvg.station && usesMvg) errors.push(["mvgStation", t("stationMissing")]);
    m.sections = [0, 1].map((i) => {
      const name = (mvg.names[i] || $("secName" + i).placeholder).trim();
      if (bytes(name) > 24) errors.push(["secName" + i, t("tooLong", { n: 24 })]);
      const lines = linesOf(mvg.pick, i);
      if (lines.length > MAX_LINES) lineError(t("tooManyLines"));
      if (!lines.length && mvg.station) lineError(t("noLinesInSection", { name }));
      return { name, lines };
    });
  } else {
    if (!mvg.station && usesMvg) errors.push(["mvgStation", t("stationMissing")]);
    if (!mvg.station2 && usesMvg) lineError(t("secondStationMissing"));
    const stations = [mvg.station, mvg.station2];
    // The widget's station is the first one.
    setOrDelete(m, "station", mvg.station ? mvg.station.id : "");
    m.sections = [0, 1].map((i) => {
      const st = stations[i];
      // The first station's title is the display name above.
      const own = i === 0 ? $("mvgName").value : mvg.names[i];
      const name = (own || (st ? st.name : "")).trim() || t("section") + " " + (i + 1);
      if (bytes(name) > 24) errors.push([i === 0 ? "mvgName" : "secName" + i, t("tooLong", { n: 24 })]);
      const pick = i === 0 ? mvg.pick : mvg.pick2;
      const s = { name };
      if (st) s.station = st.id;
      if (st && !allTicked(i, st.id, pick)) {
        const lines = linesOf(pick, true);
        if (lines.length > MAX_LINES) lineError(t("tooManyLines"));
        if (!lines.length) lineError(t("noLinesInSection", { name }));
        s.lines = lines;
      }
      return s;
    });
  }

  // Weather
  const w = c.widgets.weather = c.widgets.weather || {};
  const lat = parseNumber($("weatherLat").value), lon = parseNumber($("weatherLon").value);
  const bad = (lat === null) !== (lon === null) || Number.isNaN(lat) || Number.isNaN(lon) ||
    (lat !== null && (lat < -90 || lat > 90 || lon < -180 || lon > 180));
  if (bad) errors.push(["weatherCoordError", t("coordinatesBad")]);
  if (lat !== null && !bad) { w.latitude = lat; w.longitude = lon; } else if (!bad) { delete w.latitude; delete w.longitude; }
  setOrDelete(w, "name", check("weatherName", 32));

  // Calendar address
  if (calendarChange && !/^(https|webcal):\/\//i.test(calendarChange)) errors.push(["calendarError", t("urlBad")]);
  // Calendar: only what differs from the defaults is stored.
  const k = c.widgets.calendar = c.widgets.calendar || {};
  const days = Number($("calDays").value);
  if (days === 7) delete k.days; else k.days = days;
  const flag = (key, id, def) => { if ($(id).checked === def) delete k[key]; else k[key] = !def; };
  flag("all_day", "calAllDay", true);
  flag("past", "calPast", true);
  flag("location", "calLocation", false);
  flag("more", "calMore", false);
  flag("wrap", "calWrap", true);
  const words = $("calExclude").value.split(",").map((w) => w.trim()).filter((w) => w);
  if (words.length > CAL_MAX_WORDS) errors.push(["calExcludeError", t("calExcludeTooMany", { n: CAL_MAX_WORDS })]);
  if (words.some((w) => bytes(w) > CAL_MAX_WORD)) errors.push(["calExcludeError", t("tooLong", { n: CAL_MAX_WORD })]);
  if (words.length) k.exclude = words; else delete k.exclude;
  const every = Number($("calRefresh").value);
  if (every === 15) delete k.refresh_minutes; else k.refresh_minutes = every;
  if (!Object.keys(k).length) delete c.widgets.calendar;

  // Display
  c.layout = c.layout || {};
  c.layout.slots = [0, 1, 2].map((i) => $("slot" + i).value);
  if ($("gridEqual").checked) c.layout.grid = "equal"; else delete c.layout.grid;
  if ($("dividers").checked) delete c.layout.dividers; else c.layout.dividers = false;
  delete c.layout.template;
  c.language = $("language").value;
  c.display = c.display || {};
  c.display.full_refresh_minutes = Number($("fullRefresh").value);

  // Device
  const tz = $("timeZone").value || $("timeZoneCustom").value.trim();
  if (!tz) errors.push(["timeZoneCustom", t("required")]);
  if (tz === DEFAULT_TZ) delete c.time_zone; else c.time_zone = tz;
  const ntp = $("ntpServer").value.trim();
  if (!ntp || ntp === "pool.ntp.org") delete c.ntp_server; else c.ntp_server = ntp;
  return { config: c, errors };
}

// --- System (core/system_report.h, core/syslog.h) ------------------------------------------

// Reasons of the last restart (core::restartReason()) in German.
const RESTART_REASONS = {
  "power on": "Strom eingeschaltet", "reset pin": "Reset-Taste",
  "software restart": "Neustart durch die Firmware", "crash (panic)": "Absturz (Panic)",
  "watchdog (interrupt)": "Watchdog (Interrupt)", "watchdog (task)": "Watchdog (Task)", "watchdog": "Watchdog",
  "voltage drop (brownout)": "Spannungseinbruch", "USB (flashing or reset)": "USB (Aufspielen/Reset)",
  "power glitch": "Stromstörung",
};
const SOURCE_NAMES = { mvg: "pathMvg", weather: "pathWeather", calendar: "pathCalendar" };
const STATE_KEYS = { "ok": "stateOk", "stale": "stateStale", "waiting": "stateWaiting", "no data": "stateNoData", "not set up": "stateNotSetUp" };
// One line of GET /api/log (core::formatLogLine()).
const LOG_LINE = /^(.{19}) (INFO |WARN |ERROR) (\S+) +(.*)$/;
let sysLines = [];      // the log, oldest first
let sysFilter = "all";
let sysLoading = false;

const num = (v, digits) => Number(v).toLocaleString(lang === "en" ? "en-GB" : "de-DE", { maximumFractionDigits: digits || 0 });
function quality(rssi) {
  return t(rssi >= -55 ? "qExcellent" : rssi >= -67 ? "qGood" : rssi >= -75 ? "qFair" : "qWeak");
}
function sysTime(unix) {
  if (!unix) return "–";
  const d = new Date(unix * 1000);
  const today = new Date().toDateString() === d.toDateString();
  return today ? hhmm(d) : d.toLocaleDateString(lang === "en" ? "en-GB" : "de-DE", { day: "2-digit", month: "2-digit" }) + " " + hhmm(d);
}
function kv(label, value, cls) { return [el("dt", {}, label), el("dd", { class: cls }, value)]; }

function renderSystem(r) {
  const d = r.device || {};
  const reason = lang === "de" ? (RESTART_REASONS[d.restart_reason] || d.restart_reason) : d.restart_reason;
  $("sysInfo").replaceChildren(
    ...kv(t("sysFirmware"), d.firmware || "–"),
    ...kv(t("sysUptime"), duration(d.uptime_s || 0)),
    ...kv(t("sysRestart"), reason + (d.restart_unplanned ? " – " + t("sysUnplanned") : "") + " · " + t("sysStart", { n: d.boot || 1 }),
      d.restart_unplanned ? "error" : ""),
    ...kv(t("sysWifi"), d.wifi_connected ? t("sysConnected", { rssi: d.rssi, q: quality(d.rssi) }) : t("sysNotConnected"),
      d.wifi_connected ? (d.rssi < -75 ? "warn" : "") : "warn"),
    ...kv(t("sysClock"), t(d.time_valid ? "sysClockSet" : "sysClockNotSet"), d.time_valid ? "" : "warn"),
    ...kv(t("sysMemory"), t("sysMemoryText", { free: Math.round(d.heap_free / 1024), min: Math.round(d.heap_min_free / 1024), block: Math.round(d.heap_largest / 1024) })),
    ...kv(t("sysPsram"), t("sysPsramText", { free: num(d.psram_free / 1048576, 1), size: num(d.psram_size / 1048576, 1) })),
    ...kv(t("sysCounts"), t("sysCountsText", { w: r.log.warnings, e: r.log.errors }), r.log.errors ? "error" : r.log.warnings ? "warn" : ""));
  $("sysSources").replaceChildren(...(r.sources || []).flatMap((s) => {
    const parts = [t(STATE_KEYS[s.state] || s.state)];
    if (s.last_success) parts.push(t("sysLastOk", { time: sysTime(s.last_success), s: num(s.duration_ms / 1000, 1) }));
    if (s.failures_in_row) parts.push(t("sysFailing", { time: sysTime(s.last_failure), problem: s.problem, n: s.failures_in_row }));
    else if (s.failures) parts.push(t("sysLastFailure", { time: sysTime(s.last_failure), problem: s.problem }));
    parts.push(t("sysTotals", { ok: s.successes, failed: s.failures }));
    const bad = s.state === "stale" || s.state === "no data" || s.failures_in_row > 0;
    return kv(SOURCE_NAMES[s.name] ? t(SOURCE_NAMES[s.name]) : s.name, parts.join(" · "), bad ? "warn" : "");
  }));
}

function parseLog(text) {
  const lines = [];
  for (const line of text.split("\n")) {
    const m = LOG_LINE.exec(line);
    if (!m) continue;  // the header ("# ...") and empty lines
    lines.push({ time: m[1].trim(), level: m[2].trim(), area: m[3], text: m[4] });
  }
  return lines;
}
// "2026-11-06 07:42:05" -> "06.11. 07:42:05"; "+0:00:12" (before the clock was set) as it is.
function logTime(s) {
  const m = /^(\d{4})-(\d\d)-(\d\d) (.*)$/.exec(s);
  return m ? (lang === "en" ? m[3] + "/" + m[2] : m[3] + "." + m[2] + ".") + " " + m[4] : s;
}
function renderLog() {
  const keep = sysFilter === "error" ? ["ERROR"] : sysFilter === "warn" ? ["WARN", "ERROR"] : ["INFO", "WARN", "ERROR"];
  const rows = [];
  for (let i = sysLines.length - 1; i >= 0; i--) {
    const l = sysLines[i];
    if (!keep.includes(l.level)) continue;
    // A start begins with the entry "start ..." of the area "system"; a dashed
    // line below it separates the start before.
    const boot = l.area === "system" && l.text.startsWith("start ");
    rows.push(el("div", { class: (l.level === "ERROR" ? "error" : l.level === "WARN" ? "warn" : "") + (boot ? " boot" : "") },
      el("span", {}, logTime(l.time)), el("span", {}, l.level), el("span", {}, l.area), el("span", {}, l.text)));
  }
  $("sysLog").replaceChildren(...(rows.length ? rows : [el("p", {}, t("sysNoEntries"))]));
}
async function loadSystem() {
  if (sysLoading) return;
  sysLoading = true;
  $("sysProblem").hidden = true;
  try {
    renderSystem(await api("api/system", { passive: true }));
    const text = await api("api/log", { passive: true });
    sysLines = parseLog(typeof text === "string" ? text : "");
    renderLog();
  } catch (e) {
    if (!(e instanceof HttpError && e.status === 401)) {
      $("sysProblem").textContent = t("sysLoadFailed");
      $("sysProblem").hidden = false;
    }
  } finally {
    sysLoading = false;
  }
}
segSet($("sysFilter"), sysFilter);
$("sysFilter").addEventListener("click", (e) => {
  const b = e.target.closest("button");
  if (!b) return;
  sysFilter = b.dataset.v;
  segSet($("sysFilter"), sysFilter);
  renderLog();
});
$("sysRefresh").addEventListener("click", loadSystem);

// --- Tabs ----------------------------------------------------------------------------

const TABS = ["general", "mvg", "weather", "calendar", "device", "system"];
function selectTab(name) {
  if (!TABS.includes(name)) name = "general";
  document.querySelectorAll("#tabs button").forEach((b) => b.setAttribute("aria-selected", String(b.dataset.tab === name)));
  document.querySelectorAll("main section[data-tab]").forEach((s) => { s.hidden = s.dataset.tab !== name; });
  try { sessionStorage.setItem("inktavlaTab", name); } catch (e) { /* not available */ }
  if (name === "system") loadSystem();
}
$("tabs").addEventListener("click", (e) => {
  const b = e.target.closest("button");
  if (b) selectTab(b.dataset.tab);
});
function initialTab() {
  try { return sessionStorage.getItem("inktavlaTab") || "general"; } catch (e) { return "general"; }
}
function markTabErrors(errors) {
  const bad = new Set();
  for (const [id] of errors) {
    const s = $(id) && $(id).closest("section[data-tab]");
    if (s) bad.add(s.dataset.tab);
  }
  document.querySelectorAll("#tabs button").forEach((b) => b.classList.toggle("bad", bad.has(b.dataset.tab)));
  return bad;
}

function showFieldErrors(errors) {
  document.querySelectorAll(".bad").forEach((e) => e.classList.remove("bad"));
  for (const id of ["mvgLinesError", "weatherCoordError", "calendarError", "calExcludeError"]) {
    if ($(id)) $(id).hidden = true;
  }
  document.querySelectorAll(".fielderr").forEach((e) => e.remove());
  for (const [id, msg] of errors) {
    const target = $(id);
    if (!target) continue;
    if (target.classList.contains("err")) {
      target.textContent = msg;
      target.hidden = false;
    } else {
      if (target.tagName === "INPUT" || target.tagName === "SELECT") target.classList.add("bad");
      target.after(el("p", { class: "err fielderr" }, msg));
    }
  }
  markTabErrors(errors);
  if (errors.length) {
    const first = $(errors[0][0]);
    if (first) {
      const own = first.closest("section[data-tab]");
      if (own) selectTab(own.dataset.tab);
      const details = first.closest("details");
      if (details) details.open = true;
      first.scrollIntoView({ block: "center", behavior: "smooth" });
    }
  }
}

// German (or English) name of a setting the display reports.
function pathName(path) {
  const map = [
    ["secrets.calendar_url", "pathCalendarUrl"], ["widgets.mvg", "pathMvg"], ["widgets.weather", "pathWeather"],
    ["widgets.calendar", "pathCalendar"], ["layout", "pathLayout"], ["display", "pathLayout"],
    ["language", "pathLanguage"], ["time_zone", "pathTimeZone"], ["ntp_server", "pathNtp"],
  ];
  for (const [prefix, key] of map) if (path === prefix || path.startsWith(prefix + ".")) return t(key);
  return path ? path : t("pathFile");
}

function showIssues(issues) {
  const box = $("issues");
  const blocking = issues.some((i) => i.blocking);
  box.replaceChildren(
    el("h2", {}, t("issuesTitle")),
    el("p", { class: "hint" }, blocking ? t("issuesBlocking") : t("issuesHint")),
    el("ul", {}, issues.map((i) => el("li", {}, el("b", {}, pathName(i.path)), el("small", {}, (i.path ? i.path + ": " : "") + i.message)))),
    blocking ? null : el("div", { class: "actions" }, el("button", { type: "button", class: "danger", onclick: () => save(true) }, t("saveAnyway"))));
  box.hidden = false;
  box.scrollIntoView({ block: "start", behavior: "smooth" });
}

async function save(force) {
  const built = buildConfig();
  showFieldErrors(built.errors);
  const bar = $("bar");
  if (built.errors.length) {
    bar.className = "bar fail";
    $("barText").textContent = t("fixFirst");
    return;
  }
  const request = { config: built.config, force: !!force };
  if (calendarChange !== undefined) request.secrets = { calendar_url: calendarChange };
  saving = true;
  $("save").disabled = true;
  $("barText").textContent = t("saving");
  try {
    await api("api/config", { method: "POST", json: request });
    $("issues").hidden = true;
    waitForRestart();
  } catch (e) {
    saving = false;
    $("save").disabled = false;
    bar.className = "bar fail";
    $("barText").textContent = t("saveFailed");
    if (e.status === 422 && e.body && e.body.issues) showIssues(e.body.issues);
  }
}
$("save").addEventListener("click", () => save(false));

// After saving or restarting: wait until the display answers again, then
// load the page anew (the session survives the restart).
function waitForRestart() {
  show("restarting");
  $("restartText").textContent = t("restarting");
  const startedAt = Date.now();
  const poll = async () => {
    try {
      const r = await fetch("api/ping", { credentials: "same-origin" });
      if (r.ok || r.status === 401) { location.replace(location.pathname); return; }
    } catch (e) { /* not back yet */ }
    if (Date.now() - startedAt > 45000) $("restartText").textContent = t("restartSlow");
    setTimeout(poll, 2000);
  };
  setTimeout(poll, 6000);
}

window.addEventListener("beforeunload", (e) => {
  if (dirty && !saving) { e.preventDefault(); e.returnValue = ""; }
});

start();
