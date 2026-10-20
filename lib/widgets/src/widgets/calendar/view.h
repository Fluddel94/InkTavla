#pragma once

// View of the calendar widget in a wide tile (440 x 218) after the overall
// UI design: an agenda with "Heute" and "Morgen" (label and date on the
// left), below the rest of the week one line per event with its day. The
// next event is bold with a countdown, all-day events are black badges.

#include <string>
#include <vector>

#include "ui/canvas.h"
#include "ui/i18n.h"
#include "widgets/calendar/agenda.h"

namespace widgets::calendar {

struct ViewOptions {
  std::string title;  // tile title, e.g. "Kalender"
  std::string note;   // right part of the title, e.g. "KW 43"
  bool note_alert = false;
  ui::Language language = ui::Language::kGerman;
};

// Height of an agenda line and the space a dotted separator takes.
inline constexpr int kRowHeight = 23;
inline constexpr int kSeparatorHeight = 7;

// Short date of a day: "Di 20." / "Tue 20".
std::string shortDate(int64_t day, ui::Language language);

// Draws the agenda (from buildAgenda(); today first, then tomorrow).
void drawCalendar(ui::Canvas& canvas, const std::vector<AgendaDay>& agenda,
                  const ViewOptions& options);

}  // namespace widgets::calendar
