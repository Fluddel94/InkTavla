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
  kLoading,         // a widget waiting for its first data
  kNotSetUp,        // a column or widget without the settings it needs
  kNotAvailable,    // a widget that does not exist (yet)
  kStationMissing,  // MVG widget without a station
  kTitleDepartures,
  kTitleWeather,
  kTitleCalendar,
  kLocationMissing,         // weather widget without coordinates
  kToday,                   // agenda: "Heute"
  kTomorrow,                // agenda: "Morgen"
  kAllDay,                  // after an all-day event: "ganztägig"
  kNoEvents,                // a day without events
  kRunning,                 // an event that has started: "läuft"
  kUntil,                   // end of a multi-day event: "bis Fr"
  kIn,                      // countdown: "in 48 min"
  kHoursUnit,               // after a number of hours: "in 2 h"
  kWeekShort,               // week number in the column title: "KW 43"
  kCalendarAddressMissing,  // calendar widget without an iCal address
  kDoesNotFit,              // a widget in a column of a size it is not made for
  // Settings page open (band instead of the sentence).
  kSettingsOpen,     // "Die Einstellungsseite ist offen."
  kSettingsBrowser,  // before the address: "Im Browser:"
  kSettingsCode,     // before the one-time code: "Code:"
  kSettingsQr,       // "Oder den QR-Code scannen."
  kOpenUntil,        // before a time: "Offen bis"
  // No Wi-Fi (whole screen, set up over USB).
  kSetupHeading,           // header line: "WLAN-Einrichtung"
  kWifiNotSetUpTitle,      // "Noch kein WLAN eingerichtet."
  kWifiNotConnectedTitle,  // "Das WLAN ist nicht erreichbar."
  kWifiSaved,              // before the name of the saved network: "Gespeichert:"
  kSetupStep1,             // connect the display to a computer by USB
  kSetupStep2,             // open the Wi-Fi setup (Improv) in Chrome or Edge
  kSetupStep3,             // choose your Wi-Fi, enter its password
  kWifiRetrying,           // footer: still trying the saved network
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
