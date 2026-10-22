#pragma once

// View of the calendar widget in a column of the "Briefing" layout
// (2026-10-21):
//  - "HEUTE" with the week number, the events of today (past ones stay until
//    midnight; "Keine Termine" without any), the next one in bold;
//  - then the next day that has events ("MORGEN" or its weekday) if there
//    is room;
//  - then the following events of the week, one line each, as many as fit.
// Times in Roboto Condensed, titles in Fraunces, shortened with "…";
// all-day events with a small calendar sheet instead of the time.

#include <string>
#include <vector>

#include "ui/canvas.h"
#include "ui/i18n.h"
#include "widgets/calendar/agenda.h"
#include "widgets/calendar/widget.h"
#include "widgets/widget.h"

namespace widgets::calendar {

struct ColumnOptions {
  std::string note;  // right of "HEUTE": "KW 43", or the age badge
  bool note_alert = false;
  ui::Language language = ui::Language::kGerman;
};

// Row heights (Briefing design): today and the next day, the week lines.
inline constexpr int kColumnDayRowHeight = 27;
inline constexpr int kColumnWeekRowHeight = 22;

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
