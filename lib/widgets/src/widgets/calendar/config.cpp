#include "widgets/calendar/config.h"

#include <algorithm>
#include <iterator>

#include "ui/text.h"

namespace widgets::calendar {

namespace {

constexpr char kPath[] = "widgets.calendar";

// "exclude": a list of words (strings); empty ones are skipped.
std::vector<std::string> readExclude(JsonObjectConst section, core::ConfigIssues& issues) {
  std::vector<std::string> words;
  JsonVariantConst value = section["exclude"];
  if (value.isNull()) return words;
  const std::string path = core::configPath(kPath, "exclude");
  if (!value.is<JsonArrayConst>()) {
    issues.push_back({path, "must be a list of words, e.g. [\"Geburtstag\"]"});
    return words;
  }
  for (JsonVariantConst item : value.as<JsonArrayConst>()) {
    if (!item.is<const char*>()) {
      issues.push_back({path, "every entry must be a word in quotes, others ignored"});
      continue;
    }
    std::string word = item.as<const char*>();
    while (!word.empty() && word.back() == ' ') word.pop_back();
    while (!word.empty() && word.front() == ' ') word.erase(word.begin());
    if (word.empty()) continue;
    if (word.size() > kMaxExcludeLength) {
      issues.push_back({path, "\"" + word + "\" is longer than " +
                                  std::to_string(kMaxExcludeLength) + " bytes, ignored"});
      continue;
    }
    if (words.size() >= kMaxExcludeWords) {
      issues.push_back(
          {path, "at most " + std::to_string(kMaxExcludeWords) + " words, the others are ignored"});
      break;
    }
    words.push_back(ui::toUpper(word));
  }
  return words;
}

}  // namespace

Config parseConfig(JsonObjectConst section, core::ConfigIssues& issues) {
  Config c;
  if (section.isNull()) return c;
  for (const char* key : core::kCalendarSecretKeys) {
    if (section[key].isNull()) continue;
    issues.push_back(
        {core::configPath(kPath, key),
         "removed when saving: the iCal address is secret and does not belong in this file; "
         "enter it on the settings page or with: tools/inktavla_config.py calendar-url"});
  }
  core::reportUnknownKeys(section,
                          {"days", "all_day", "past", "more", "location", "wrap", "exclude",
                           "refresh_minutes", "url", "ical", "ical_url", "address", "band"},
                          kPath, issues);
  DisplayConfig& d = c.display;
  JsonVariantConst days = section["days"];
  if (!days.isNull()) {
    const bool valid = days.is<int>() && std::find(std::begin(kDayChoices), std::end(kDayChoices),
                                                   days.as<int>()) != std::end(kDayChoices);
    if (valid) {
      d.days = days.as<int>();
    } else {
      issues.push_back({core::configPath(kPath, "days"), "must be 1, 2, 3 or 7"});
    }
  }
  d.all_day = core::readBool(section, "all_day", d.all_day, kPath, issues);
  d.past = core::readBool(section, "past", d.past, kPath, issues);
  d.more = core::readBool(section, "more", d.more, kPath, issues);
  d.location = core::readBool(section, "location", d.location, kPath, issues);
  d.wrap = core::readBool(section, "wrap", d.wrap, kPath, issues);
  d.exclude = readExclude(section, issues);
  c.calendar.refresh_minutes = core::readInt(section, "refresh_minutes", c.calendar.refresh_minutes,
                                             kMinRefreshMinutes, kMaxRefreshMinutes, kPath, issues);
  return c;
}

bool excluded(const DisplayConfig& config, std::string_view title) {
  if (config.exclude.empty()) return false;
  const std::string upper = ui::toUpper(title);
  for (const std::string& word : config.exclude) {
    if (upper.find(word) != std::string::npos) return true;
  }
  return false;
}

}  // namespace widgets::calendar
