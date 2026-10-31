#include "ui/i18n.h"

namespace ui {

namespace {

constexpr const char* kWeekdaysDe[] = {"Sonntag",    "Montag",  "Dienstag", "Mittwoch",
                                       "Donnerstag", "Freitag", "Samstag"};
constexpr const char* kWeekdaysEn[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                       "Thursday", "Friday", "Saturday"};
constexpr const char* kMonthsDe[] = {"Januar",    "Februar", "März",     "April",
                                     "Mai",       "Juni",    "Juli",     "August",
                                     "September", "Oktober", "November", "Dezember"};
constexpr const char* kMonthsEn[] = {"January",   "February", "March",    "April",
                                     "May",       "June",     "July",     "August",
                                     "September", "October",  "November", "December"};

constexpr const char* kWeekdaysShortDe[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
constexpr const char* kWeekdaysShortEn[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

}  // namespace

const char* weekdayShort(Language language, int weekday) {
  if (weekday < 0 || weekday > 6) return "";
  return language == Language::kGerman ? kWeekdaysShortDe[weekday] : kWeekdaysShortEn[weekday];
}

Language languageFromCode(std::string_view code) {
  return code == "en" ? Language::kEnglish : Language::kGerman;
}

const char* text(Language language, Text id) {
  const bool de = language == Language::kGerman;
  switch (id) {
    case Text::kNow:
      return de ? "jetzt" : "now";
    case Text::kMinutesUnit:
      return "min";
    case Text::kNoDepartures:
      return de ? "Keine Abfahrten" : "No departures";
    case Text::kMoreIncidents:
      return de ? "weitere" : "more";
    case Text::kAsOf:
      return de ? "Stand" : "As of";
    case Text::kNoConnection:
      return de ? "Keine Verbindung" : "No connection";
    case Text::kNoConnectionDetail:
      return de ? "Wartet auf WLAN" : "Waiting for Wi-Fi";
    case Text::kNoData:
      return de ? "Keine Daten" : "No data";
    case Text::kLoading:
      return de ? "Wird geladen …" : "Loading …";
    case Text::kNotSetUp:
      return de ? "Nicht eingerichtet" : "Not set up";
    case Text::kNotAvailable:
      return de ? "Noch nicht verfügbar" : "Not available yet";
    case Text::kStationMissing:
      return de ? "Haltestelle fehlt" : "Station missing";
    case Text::kTitleDepartures:
      return de ? "Abfahrten" : "Departures";
    case Text::kTitleWeather:
      return de ? "Wetter" : "Weather";
    case Text::kTitleCalendar:
      return de ? "Kalender" : "Calendar";
    case Text::kLocationMissing:
      return de ? "Standort fehlt" : "Location missing";
    case Text::kToday:
      return de ? "Heute" : "Today";
    case Text::kTomorrow:
      return de ? "Morgen" : "Tomorrow";
    case Text::kAllDay:
      return de ? "ganztägig" : "all day";
    case Text::kNoEvents:
      return de ? "Keine Termine" : "No events";
    case Text::kRunning:
      return de ? "läuft" : "now";
    case Text::kUntil:
      return de ? "bis" : "until";
    case Text::kIn:
      return "in";
    case Text::kHoursUnit:
      return "h";
    case Text::kWeekShort:
      return de ? "KW" : "Wk";
    case Text::kCalendarAddressMissing:
      return de ? "Kalender-Adresse fehlt" : "Calendar address missing";
    case Text::kCalendarCheckAddress:
      return de ? "Kalender-Adresse prüfen" : "Check the calendar address";
    case Text::kCalendarUnreachable:
      return de ? "Kalender nicht erreichbar" : "Calendar not reachable";
    case Text::kDoesNotFit:
      return de ? "Passt hier nicht" : "Does not fit here";
    case Text::kSettingsOpen:
      return de ? "Die Einstellungsseite ist offen." : "The settings page is open.";
    case Text::kSettingsBrowser:
      return de ? "Im Browser:" : "In a browser:";
    case Text::kSettingsCode:
      return "Code:";
    case Text::kSettingsQr:
      return de ? "Oder den QR-Code scannen." : "Or scan the QR code.";
    case Text::kOpenUntil:
      return de ? "Offen bis" : "Open until";
    case Text::kSetupHeading:
      return de ? "WLAN-Einrichtung" : "Wi-Fi setup";
    case Text::kWifiNotSetUpTitle:
      return de ? "Noch kein WLAN eingerichtet." : "No Wi-Fi set up yet.";
    case Text::kWifiNotConnectedTitle:
      return de ? "Das WLAN ist nicht erreichbar." : "The Wi-Fi cannot be reached.";
    case Text::kWifiSaved:
      return de ? "Gespeichert:" : "Saved:";
    case Text::kSetupStep1:
      return de ? "Das Display per USB-Kabel mit einem Computer verbinden."
                : "Connect the display to a computer with a USB cable.";
    case Text::kSetupStep2:
      return de ? "In Chrome oder Edge die WLAN-Einrichtung (Improv) öffnen und das Display "
                  "auswählen."
                : "Open the Wi-Fi setup (Improv) in Chrome or Edge and choose the display.";
    case Text::kSetupStep3:
      return de ? "Dein WLAN auswählen, Passwort eingeben, verbinden."
                : "Choose your Wi-Fi, enter its password, connect.";
    case Text::kWifiRetrying:
      return de ? "Das Display versucht es weiter mit dem gespeicherten WLAN."
                : "The display keeps trying the saved Wi-Fi.";
    case Text::kStartVersion:
      return "Version";
    case Text::kStartLoading:
      return de ? "Daten werden geladen …" : "Loading data …";
    case Text::kStartConnecting:
      return de ? "Verbinde mit WLAN" : "Connecting to Wi-Fi";
    case Text::kWifiFailedTitle:
      return de ? "WLAN-Verbindung fehlgeschlagen." : "Wi-Fi connection failed.";
    case Text::kWifiTried:
      return de ? "Versucht:" : "Tried:";
    case Text::kWifiCheckPassword:
      return de ? "Passwort prüfen und das WLAN per USB neu einrichten."
                : "Check the password and set up the Wi-Fi again over USB.";
    case Text::kSetUpOnSettingsPage:
      return de ? "Auf der Einstellungsseite einrichten (Taste 3)"
                : "Set up on the settings page (button 3)";
    case Text::kCheckOnSettingsPage:
      return de ? "Auf der Einstellungsseite prüfen (Taste 3)"
                : "Check on the settings page (button 3)";
  }
  return "";
}

const char* weekdayName(Language language, int weekday) {
  if (weekday < 0 || weekday > 6) return "";
  return language == Language::kGerman ? kWeekdaysDe[weekday] : kWeekdaysEn[weekday];
}

const char* monthName(Language language, int month) {
  if (month < 1 || month > 12) return "";
  return language == Language::kGerman ? kMonthsDe[month - 1] : kMonthsEn[month - 1];
}

std::string longDate(Language language, int weekday, int day, int month) {
  std::string out = weekdayName(language, weekday);
  out += ", ";
  out += std::to_string(day);
  out += language == Language::kGerman ? ". " : " ";
  out += monthName(language, month);
  return out;
}

}  // namespace ui
