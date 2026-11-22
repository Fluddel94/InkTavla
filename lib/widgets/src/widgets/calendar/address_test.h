#pragma once

// "Adresse testen" on the settings page (POST /api/calendar/test): requests
// an iCal address once and says whether it works and how many events lie in
// the next days. The address is a secret: it is neither part of the result
// nor logged.

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "widgets/calendar/widget.h"

namespace widgets::calendar {

// Days counted from today on (today included).
inline constexpr int kTestDays = 7;
// The test stops a request after this time (a normal fetch has no limit).
inline constexpr uint32_t kTestTimeoutMs = 30000;

enum class TestOutcome : uint8_t {
  kOk,
  kAddress,  // rejected by the server (400/401/403/404/410)
  kNotIcal,  // the answer is no iCal file (e.g. a sign-in page)
  kNetwork,  // no connection, server error, file cut off
  kTimeout,  // longer than kTestTimeoutMs
  kNoTime,   // the clock is not set yet: the days are unknown
};

struct AddressTest {
  TestOutcome outcome = TestOutcome::kNetwork;
  size_t events = 0;  // occurrences in the kTestDays days (kOk only)
};

// Requests the calendar from source at wall_s (UTC) and counts its
// occurrences on the local days today .. today + kTestDays - 1, without the
// word filter of the widget.
AddressTest testAddress(Source& source, int64_t wall_s);

// Name of an outcome in the reply ("address", "not_ical" ...).
const char* testOutcomeName(TestOutcome outcome);

// The reply: {"ok":true,"events":12,"days":7} or {"ok":false,"error":"..."}.
std::string addressTestJson(const AddressTest& test);

#if defined(ARDUINO)
// Source requesting the given address (https:// or webcal://) over HTTPS,
// stopped after kTestTimeoutMs.
class UrlSource : public Source {
 public:
  explicit UrlSource(std::string url) : url_(std::move(url)) {}
  bool configured() const override { return !url_.empty(); }
  bool fetchCalendar(Parser& parser) override;
  FetchError lastError() const override { return error_; }

 private:
  std::string url_;
  FetchError error_ = FetchError::kNone;
};
#endif

}  // namespace widgets::calendar
