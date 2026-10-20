#pragma once

// The calendar widget: reads the private iCal address of a calendar (Google
// Calendar, Outlook, Nextcloud, iCloud) every 15 minutes and shows an agenda
// of today, tomorrow and the rest of the week.

#include <string>

#include "widgets/calendar/agenda.h"
#include "widgets/calendar/parser.h"
#include "widgets/widget.h"

namespace widgets::calendar {

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
};

// Days read beyond the agenda, so that after midnight the next days are
// still there if a request fails.
inline constexpr int kExtraDays = 2;

class CalendarWidget : public Widget {
 public:
  explicit CalendarWidget(Source& source);

  const char* id() const override { return "calendar"; }
  std::string title(ui::Language language) const override;
  SetupMessage setupMessage(ui::Language language) const override;

  bool fetches() const override { return true; }
  // Every 15 minutes, retries every minute.
  core::Schedule schedule() const override { return {900, 0, 60}; }
  bool isSetUp() const override { return source_.configured(); }
  // Needs the time (the window starts today); false before NTP has answered.
  bool fetch(int64_t wall_s) override;

  void draw(ui::Canvas& canvas, const DrawContext& context) override;

 private:
  Source& source_;
  Occurrences occurrences_;
};

}  // namespace widgets::calendar
