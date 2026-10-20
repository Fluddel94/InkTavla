#pragma once

#include <string>
#include <string_view>

namespace ui {

// Language of texts on the display (configuration setting "language").
enum class Language { kGerman, kEnglish };

// "de" -> kGerman, "en" -> kEnglish; anything else kGerman.
Language languageFromCode(std::string_view code);

enum class Text {
  kNow,           // departure in this minute
  kMinutesUnit,   // after a number of minutes: "4 min"
  kNoDepartures,  // empty departure list
  kAsOf,          // age hint: "Stand 14:32"
  kNoConnection,  // a widget has never received data
  kNoConnectionDetail,
  kNoData,          // a widget's data could not be read
  kNotSetUp,        // a slot or widget without the settings it needs
  kNotAvailable,    // a widget that does not exist (yet)
  kStationMissing,  // MVG widget without a station
  kTitleDepartures,
  kTitleWeather,
  kTitleCalendar,
  kFeelsLike,        // "gefühlt 5°"
  kClockSuffix,      // after a time range: "14–18 Uhr"
  kNoRain,           // rain outlook without rain
  kLocationMissing,  // weather widget without coordinates
};

const char* text(Language language, Text id);

// Day of the week (0 = Sunday .. 6 = Saturday) and month (1..12); "" if out
// of range.
const char* weekdayName(Language language, int weekday);
const char* monthName(Language language, int month);

// Short day of the week for tables: "Mi" / "Wed".
const char* weekdayShort(Language language, int weekday);

// Long date for the header: "Dienstag, 20. Oktober" / "Tuesday, 20 October".
std::string longDate(Language language, int weekday, int day, int month);

}  // namespace ui
