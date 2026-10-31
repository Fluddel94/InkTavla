#include "widgets/calendar/api.h"

#if defined(ARDUINO)

#include <Arduino.h>

#include <string>

#include "core/http.h"
#include "core/secrets.h"

namespace widgets::calendar {

void ApiSource::begin() {
  core::NvsSecretStore secrets;
  configured_ = secrets.has(core::kSecretCalendarUrl);
}

bool ApiSource::fetchCalendar(Parser& parser) {
  core::NvsSecretStore secrets;
  std::string url;
  if (!secrets.read(core::kSecretCalendarUrl, url) || url.empty()) return false;
  core::HttpResult result;
  const uint32_t start = millis();
  const bool ok = core::fetchStream(
      core::secretUrl(url),
      [&](const char* data, size_t length) {
        parser.feed(data, length);
        // No iCal file (e.g. an HTML sign-in page): stop at once.
        return !parser.notCalendar();
      },
      kMaxCalendarBytes, &result);
  error_ = ok ? FetchError::kNone : errorForStatus(result.status);
  if (!ok) {
    Serial.printf("Calendar: request failed (%s)\n", parser.notCalendar() ? "no iCal file"
                                                     : error_ == FetchError::kAddress
                                                         ? "address rejected"
                                                         : "network");
  }
  // Only sizes and counts in the log, never the address or titles.
  if (ok) {
    Serial.printf("Calendar: %u bytes in %lu ms, %u events read, %u kept%s\n",
                  static_cast<unsigned>(result.bytes), static_cast<unsigned long>(millis() - start),
                  static_cast<unsigned>(parser.eventsRead()),
                  static_cast<unsigned>(parser.events().size()),
                  parser.truncated() ? " (limit reached)" : "");
  }
  return ok;
}

}  // namespace widgets::calendar

#endif
