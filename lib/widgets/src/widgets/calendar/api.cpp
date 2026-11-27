#include "widgets/calendar/api.h"

#if defined(ARDUINO)

#include <Arduino.h>

#include <string>

#include "core/http.h"
#include "core/secrets.h"
#include "core/syslog.h"
#include "widgets/calendar/address_test.h"

namespace widgets::calendar {

void ApiSource::begin() {
  core::NvsSecretStore secrets;
  configured_ = secrets.has(core::kSecretCalendarUrl);
}

namespace {

// Requests url into parser and stops the request after limit_ms (error
// kTimeout).
bool fetchUrl(const std::string& url, Parser& parser, FetchError& error, uint32_t limit_ms) {
  core::HttpResult result;
  const uint32_t start = millis();
  bool late = false;
  const bool ok = core::fetchStream(
      core::secretUrl(url),
      [&](const char* data, size_t length) {
        if (millis() - start > limit_ms) {
          late = true;
          return false;
        }
        parser.feed(data, length);
        // No iCal file (e.g. an HTML sign-in page): stop at once.
        return !parser.notCalendar();
      },
      kMaxCalendarBytes, &result);
  error = ok ? FetchError::kNone : late ? FetchError::kTimeout : errorForStatus(result.status);
  if (!ok) {
    Serial.printf("Calendar: request failed (%s)\n", parser.notCalendar() ? "no iCal file"
                                                     : late               ? "time limit"
                                                     : error == FetchError::kAddress
                                                         ? "address rejected"
                                                         : "network");
    // For the system log; the HTTP status is noted by core::fetchStream().
    if (parser.notCalendar()) core::noteFetchProblem("no iCal file (address wrong?)");
    if (late) {
      core::noteFetchProblem("took longer than %u s", static_cast<unsigned>(limit_ms / 1000));
    }
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

}  // namespace

bool ApiSource::fetchCalendar(Parser& parser) {
  core::NvsSecretStore secrets;
  std::string url;
  if (!secrets.read(core::kSecretCalendarUrl, url) || url.empty()) return false;
  return fetchUrl(url, parser, error_, kFetchLimitMs);
}

bool UrlSource::fetchCalendar(Parser& parser) {
  return fetchUrl(url_, parser, error_, kTestTimeoutMs);
}

}  // namespace widgets::calendar

#endif
