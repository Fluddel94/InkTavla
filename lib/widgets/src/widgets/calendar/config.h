#pragma once

// Configuration of the calendar widget: section "widgets" -> "calendar" of
// the configuration file (see docs/configuration.md). The iCal address is a
// secret and kept in NVS (core/secrets.h), never in the file. Without any
// setting the column looks as before these settings existed (2026-11-05).

#include <ArduinoJson.h>

#include <string>
#include <string_view>
#include <vector>

#include "core/config.h"

namespace widgets::calendar {

// Limits of the settings.
// Days of the agenda: today, today and tomorrow, three days or a week.
inline constexpr int kDayChoices[] = {1, 2, 3, 7};
inline constexpr int kMinRefreshMinutes = 5;
inline constexpr int kMaxRefreshMinutes = 360;
inline constexpr size_t kMaxExcludeWords = 10;
inline constexpr size_t kMaxExcludeLength = 32;  // bytes

// Settings of one calendar: where its events come from. There is only one
// calendar so far (its address is the secret "calendar_url"); a list of
// calendars can be added later without changing the display settings.
struct CalendarConfig {
  int refresh_minutes = 15;
};

// What the column shows (for all calendars).
struct DisplayConfig {
  int days = 7;                      // today and the following days
  bool all_day = true;               // all-day and multi-day events
  bool past = true;                  // events of today that are over (until midnight)
  bool more = false;                 // "+2 weitere" below today and the next day
  bool location = false;             // place below the title (today and the next day)
  bool wrap = true;                  // long titles wrap once onto a second line
  std::vector<std::string> exclude;  // events whose title contains a word
};

struct Config {
  CalendarConfig calendar;
  DisplayConfig display;
};

// Reads the section; invalid values are reported under "widgets.calendar"
// and replaced by their defaults. An address put into the file by mistake
// gets a hint how to set it instead.
Config parseConfig(JsonObjectConst section, core::ConfigIssues& issues);

// True if the title contains one of the words (ignoring the case of ASCII
// letters and of ÄÖÜ).
bool excluded(const DisplayConfig& config, std::string_view title);

}  // namespace widgets::calendar
