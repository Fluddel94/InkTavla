#include "widgets/calendar/config.h"

namespace widgets::calendar {

namespace {

constexpr char kPath[] = "widgets.calendar";

}  // namespace

void checkConfig(JsonObjectConst section, core::ConfigIssues& issues) {
  if (section.isNull()) return;
  for (JsonPairConst kv : section) {
    const std::string key = kv.key().c_str();
    if (key == "url" || key == "ical" || key == "ical_url" || key == "address") {
      issues.push_back(
          {core::configPath(kPath, key.c_str()),
           "ignored: the iCal address is secret and does not belong in this file; set it "
           "with: tools/inktavla_config.py calendar-url"});
    } else {
      issues.push_back({core::configPath(kPath, key.c_str()), "unknown setting, ignored"});
    }
  }
}

}  // namespace widgets::calendar
