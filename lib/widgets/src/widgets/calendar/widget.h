#pragma once

// The calendar widget: a data source that reads the private iCal address of
// a calendar (Google Calendar, Outlook, Nextcloud, iCloud) every 15 minutes
// (setting refresh_minutes)
// and a view that shows an agenda of today, tomorrow and the rest of the
// week.

#include <cstdint>
#include <string>
#include <vector>

#include "widgets/calendar/agenda.h"
#include "widgets/calendar/config.h"
#include "widgets/calendar/parser.h"
#include "widgets/widget.h"

namespace widgets::calendar {

// Why a request for the calendar failed.
enum class FetchError : uint8_t {
  kNone,
  kNetwork,  // no connection, timeout, server error: tried again
  kAddress,  // the address is wrong: rejected (401/403/404/410) or no iCal file
  kTimeout,  // stopped after a time limit (only the address test, address_test.h)
};

// Kind of error for the HTTP status of a failed request (negative: error
// of the HTTP client).
FetchError errorForStatus(int status);

// Where the calendar comes from: the iCal address on the device, saved files
// in tests.
class Source {
 public:
  virtual ~Source() = default;
  // True if an address is set.
  virtual bool configured() const = 0;
  // Feeds the whole iCal file into parser as it arrives (without
  // finish()). False if the request failed.
  virtual bool fetchCalendar(Parser& parser) = 0;
  // Why the last fetchCalendar() failed (before the parser saw anything
  // wrong with the file).
  virtual FetchError lastError() const { return FetchError::kNetwork; }
};

// Days read beyond the agenda, so that after midnight the next days are
// still there if a request fails.
inline constexpr int kExtraDays = 2;

// Data source: the occurrences of the coming days.
class CalendarSource : public DataSource {
 public:
  // api must outlive the source.
  explicit CalendarSource(Source& api, Config config = {});

  const char* id() const override { return "calendar"; }
  SetupMessage setupMessage(ui::Language language) const override;

  bool fetches() const override { return true; }
  // Every refresh_minutes (default 15), retries every minute.
  core::Schedule schedule() const override {
    return {static_cast<uint32_t>(config_.calendar.refresh_minutes) * 60, 0, 60};
  }
  bool isSetUp() const override { return api_.configured(); }
  // Needs the time (the window starts today); false before NTP has answered.
  bool fetch(int64_t wall_s) override;
  // "Kalender-Adresse prüfen" or "Kalender nicht erreichbar" after a failed
  // update.
  Problem problem(ui::Language language) const override;
  FetchError lastError() const { return error_; }

  // Occurrences of today and the following days - 1 + kExtraDays days (as of
  // the last successful update).
  const Occurrences& occurrences() const { return occurrences_; }
  const Config& config() const { return config_; }

 private:
  Source& api_;
  Config config_;
  Occurrences occurrences_;
  FetchError error_ = FetchError::kNone;
};

}  // namespace widgets::calendar
