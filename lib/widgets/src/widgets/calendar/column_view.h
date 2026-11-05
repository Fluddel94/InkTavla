#pragma once

// View of the calendar widget in a column of the "Briefing" layout
// (2026-10-21):
//  - "HEUTE" with the week number, the events of today (past ones stay until
//    midnight; "Keine Termine" without any);
//  - then the next day that has events ("MORGEN" or its weekday) if there
//    is room;
//  - then the following events of the week, as many as fit: the weekday on
//    the first line of each day, then the time and the title
//    (2026-11-05; "MORGEN" has only the weekday on the right, "Mi").
// Times in Roboto Condensed, titles in Fraunces, all in the same weight
// (2026-10-25: the next event is no longer bold). A long title wraps once
// onto a second line if there is room (today: as long as all events of today
// that fit in single lines still fit), else it is shortened with "…";
// all-day events with a small calendar sheet instead of the time. Settings
// (2026-11-05): the place below the title and a hint "+ 2 weitere Termine"
// below today and the next day if events are left out; titles in single
// lines only.

#include <string>
#include <vector>

#include "ui/canvas.h"
#include "ui/i18n.h"
#include "widgets/calendar/agenda.h"
#include "widgets/calendar/config.h"
#include "widgets/calendar/widget.h"
#include "widgets/widget.h"

namespace widgets::calendar {

struct ColumnOptions {
  std::string note;  // right of "HEUTE": "KW 43", or the age badge
  bool note_alert = false;
  ui::Language language = ui::Language::kGerman;
  DisplayConfig display;  // wrap, location, more (the agenda is built with it)
};

// Row heights (Briefing design): today and the next day, the week lines.
inline constexpr int kColumnDayRowHeight = 27;
inline constexpr int kColumnWeekRowHeight = 22;
// Extra height of a title that wraps onto a second line.
inline constexpr int kColumnDayLineHeight = 22;
inline constexpr int kColumnWeekLineHeight = 20;
// Place below the title (setting "location") and the hint "+ 2 weitere
// Termine" below today or the next day (setting "more").
inline constexpr int kColumnLocationLineHeight = 18;
inline constexpr int kColumnMoreLineHeight = 20;

// Draws the agenda (from buildAgenda(); today first).
void drawAgendaColumn(ui::Canvas& canvas, const std::vector<AgendaDay>& agenda,
                      const ColumnOptions& options);

// Short date of a day: "Di 20." / "Tue 20".
std::string shortDate(int64_t day, ui::Language language);

class AgendaColumnView : public View {
 public:
  // source must outlive the view.
  explicit AgendaColumnView(const CalendarSource& source) : source_(source) {}

  std::vector<const DataSource*> sources() const override { return {&source_}; }
  std::string title(ui::Language language) const override;
  void draw(ui::Canvas& canvas, const DrawContext& context) override;

 private:
  const CalendarSource& source_;
};

}  // namespace widgets::calendar
