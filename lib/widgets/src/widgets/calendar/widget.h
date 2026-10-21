#pragma once

// The calendar widget: a data source that reads the private iCal address of
// a calendar (Google Calendar, Outlook, Nextcloud, iCloud) every 15 minutes
// and a view that shows an agenda of today, tomorrow and the rest of the
// week.

#include <string>
#include <vector>

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

// Data source: the occurrences of the coming days.
class CalendarSource : public DataSource {
 public:
  // api must outlive the source.
  explicit CalendarSource(Source& api);

  const char* id() const override { return "calendar"; }
  SetupMessage setupMessage(ui::Language language) const override;

  bool fetches() const override { return true; }
  // Every 15 minutes, retries every minute.
  core::Schedule schedule() const override { return {900, 0, 60}; }
  bool isSetUp() const override { return api_.configured(); }
  // Needs the time (the window starts today); false before NTP has answered.
  bool fetch(int64_t wall_s) override;

  // Occurrences of today and the following kAgendaDays - 1 + kExtraDays days
  // (as of the last successful update).
  const Occurrences& occurrences() const { return occurrences_; }

 private:
  Source& api_;
  Occurrences occurrences_;
};

// View in a wide tile (style A), see calendar/view.h.
class AgendaView : public View {
 public:
  // source must outlive the view.
  explicit AgendaView(const CalendarSource& source) : source_(source) {}

  std::vector<const DataSource*> sources() const override { return {&source_}; }
  std::string title(ui::Language language) const override;
  void draw(ui::Canvas& canvas, const DrawContext& context) override;

 private:
  const CalendarSource& source_;
};

}  // namespace widgets::calendar
