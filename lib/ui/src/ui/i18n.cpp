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
    case Text::kAsOf:
      return de ? "Stand" : "As of";
    case Text::kNoConnection:
      return de ? "Keine Verbindung" : "No connection";
    case Text::kNoConnectionDetail:
      return de ? "Wartet auf WLAN" : "Waiting for Wi-Fi";
    case Text::kNoData:
      return de ? "Keine Daten" : "No data";
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
    case Text::kFeelsLike:
      return de ? "gefühlt" : "feels";
    case Text::kClockSuffix:
      return de ? " Uhr" : " h";
    case Text::kNoRain:
      return de ? "Kein Regen" : "No rain";
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
    case Text::kMoreEvents:
      return de ? "weitere" : "more";
    case Text::kWeekShort:
      return de ? "KW" : "Wk";
    case Text::kCalendarAddressMissing:
      return de ? "Kalender-Adresse fehlt" : "Calendar address missing";
    case Text::kDoesNotFit:
      return de ? "Passt hier nicht" : "Does not fit here";
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
