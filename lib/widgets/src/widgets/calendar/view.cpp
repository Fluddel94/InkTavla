#include "widgets/calendar/view.h"

#include <algorithm>

#include "ui/elements.h"
#include "ui/fonts.h"
#include "ui/text.h"
#include "widgets/calendar/datetime.h"

namespace widgets::calendar {

namespace {

using ui::Color;

// Positions from the overall UI design (2026-10-20), relative to the tile.
constexpr int kX0 = ui::kTilePadding;
constexpr int kTop = 36;            // top of the first line
constexpr int kBaseline = 19;       // baseline within a line
constexpr int kMinLabelWidth = 76;  // label column ("Morgen", "Do 22.")
constexpr int kGap = 10;            // between columns
constexpr int kBadgePadding = 9;    // text inset of an all-day badge
constexpr int kNoteGap = 8;         // between a title and the note on the right

const ui::Font& labelFont() { return ui::fonts::kRobotoCondensedBold21; }
const ui::Font& subFont() { return ui::fonts::kRobotoCondensedMedium18; }
const ui::Font& entryFont() { return ui::fonts::kRobotoCondensedMedium22; }
const ui::Font& boldFont() { return ui::fonts::kRobotoCondensedBold22; }

// One line of the agenda as it is drawn.
struct Line {
  std::string label;  // left column, bold
  bool sub = false;   // label is the date below "Heute"/"Morgen" (medium)
  const Entry* entry = nullptr;
  int more = 0;  // > 0: "+n weitere" instead of an entry
};

// Lines of a group (today, tomorrow or the rest of the week) when it may
// use `rows` lines.
void addGroup(std::vector<Line>& lines, const std::vector<const AgendaDay*>& days, bool named,
              const std::string& name, int rows, ui::Language language) {
  struct Item {
    int64_t day;
    const Entry* entry;
  };
  std::vector<Item> items;
  for (const AgendaDay* d : days) {
    for (const Entry& e : d->entries) items.push_back({d->day, &e});
  }
  // Without enough space, events of today that are over go first.
  for (size_t i = 0; i < items.size() && items.size() > static_cast<size_t>(rows);) {
    if (items[i].entry->past) {
      items.erase(items.begin() + static_cast<std::ptrdiff_t>(i));
    } else {
      i++;
    }
  }
  const size_t total = items.size();
  const size_t shown = total <= static_cast<size_t>(rows) ? total : rows - 1;
  const size_t first = lines.size();
  for (size_t i = 0; i < shown; i++) {
    Line line;
    line.entry = items[i].entry;
    if (!named && (i == 0 || items[i - 1].day != items[i].day)) {
      line.label = shortDate(items[i].day, language);
    }
    lines.push_back(line);
  }
  if (shown < total) {
    Line line;
    line.more = static_cast<int>(total - shown);
    lines.push_back(line);
  }
  if (named) {
    while (lines.size() - first < 2) lines.push_back(Line());
    lines[first].label = name;
    lines[first + 1].label = shortDate(days.front()->day, language);
    lines[first + 1].sub = true;
  }
}

int groupRows(const std::vector<const AgendaDay*>& days, bool named) {
  size_t n = 0;
  for (const AgendaDay* d : days) n += d->entries.size();
  return std::max<int>(static_cast<int>(n), named ? 2 : 1);
}

void drawEntry(ui::Canvas& canvas, const Entry& e, int x_time, int x_title, int x1, int baseline,
               ui::Language language) {
  (void)language;
  const int top = baseline - kBaseline;
  switch (e.kind) {
    case Entry::Kind::kTimed: {
      const ui::Font& font = e.next ? boldFont() : entryFont();
      ui::drawText(canvas, font, x_time, baseline, e.time);
      int right = x1;
      if (!e.note.empty()) {
        ui::drawText(canvas, subFont(), x1, baseline, e.note, Color::kBlack, ui::Align::kRight);
        right = x1 - ui::textWidth(subFont(), e.note) - kNoteGap;
      }
      ui::drawText(canvas, font, x_title, baseline, ui::fitText(font, e.title, right - x_title));
      break;
    }
    case Entry::Kind::kAllDay: {
      const int suffix_w = ui::textWidth(subFont(), e.suffix);
      const int max_badge = x1 - (x_time - 2) - suffix_w - 6;
      const std::string title = ui::fitText(boldFont(), e.title, max_badge - 2 * kBadgePadding);
      const int badge_w = ui::textWidth(boldFont(), title) + 2 * kBadgePadding;
      canvas.fillRoundRect(x_time - 2, top + 1, badge_w, kRowHeight - 2, 4, Color::kBlack);
      ui::drawText(canvas, boldFont(), x_time - 2 + kBadgePadding, baseline, title, Color::kWhite);
      ui::drawText(canvas, subFont(), x_time - 2 + badge_w + 6, baseline, e.suffix);
      break;
    }
    case Entry::Kind::kNone:
      ui::drawText(canvas, entryFont(), x_time, baseline, e.title);
      break;
  }
}

}  // namespace

std::string shortDate(int64_t day, ui::Language language) {
  int y, m, d;
  civilFromDays(day, y, m, d);
  std::string out = ui::weekdayShort(language, weekdayFromDays(day));
  out += ' ';
  out += std::to_string(d);
  if (language == ui::Language::kGerman) out += '.';
  return out;
}

void drawCalendar(ui::Canvas& canvas, const std::vector<AgendaDay>& agenda,
                  const ViewOptions& options) {
  ui::drawTileTitle(canvas, options.title, options.note, options.note_alert);
  if (agenda.empty()) return;
  const ui::Language language = options.language;
  const int x1 = canvas.width() - ui::kTilePadding;

  // Groups: today, tomorrow, the rest of the week.
  std::vector<const AgendaDay*> today{&agenda[0]};
  std::vector<const AgendaDay*> tomorrow;
  std::vector<const AgendaDay*> rest;
  for (size_t i = 1; i < agenda.size(); i++) {
    (i == 1 ? tomorrow : rest).push_back(&agenda[i]);
  }

  // Lines per group: today first, but tomorrow keeps its two lines and the
  // rest of the week at least one. A single line for several later events
  // would only say "+n weitere" without a day; then the rest is left out and
  // tomorrow gets the space.
  int rows_today = 0;
  int rows_tomorrow = 0;
  int rows_rest = 0;
  for (int attempt = 0; attempt < 2; attempt++) {
    const int groups = 1 + (tomorrow.empty() ? 0 : 1) + (rest.empty() ? 0 : 1);
    int capacity = (canvas.height() - kTop - (groups - 1) * kSeparatorHeight) / kRowHeight;
    const int reserve_tomorrow = tomorrow.empty() ? 0 : 2;
    const int reserve_rest = rest.empty() ? 0 : 1;
    rows_today =
        std::min(groupRows(today, true), std::max(2, capacity - reserve_tomorrow - reserve_rest));
    capacity -= rows_today;
    rows_tomorrow = tomorrow.empty()
                        ? 0
                        : std::min(groupRows(tomorrow, true), std::max(2, capacity - reserve_rest));
    capacity -= rows_tomorrow;
    rows_rest = rest.empty() ? 0 : std::min(groupRows(rest, false), capacity);
    if (rest.empty() || rows_rest > 1 || groupRows(rest, false) == 1) break;
    rest.clear();
    rows_rest = 0;
  }

  std::vector<std::vector<Line>> blocks(3);
  addGroup(blocks[0], today, true, ui::text(language, ui::Text::kToday), rows_today, language);
  if (rows_tomorrow > 0) {
    addGroup(blocks[1], tomorrow, true, ui::text(language, ui::Text::kTomorrow), rows_tomorrow,
             language);
  }
  if (rows_rest > 0) addGroup(blocks[2], rest, false, "", rows_rest, language);

  // Columns: labels, times, titles.
  int label_w = kMinLabelWidth - kGap;
  for (const auto& block : blocks) {
    for (const Line& l : block) {
      if (!l.sub) label_w = std::max(label_w, ui::textWidth(labelFont(), l.label));
    }
  }
  const int x_time = kX0 + label_w + kGap;
  const int x_title = x_time + ui::textWidth(boldFont(), "00:00") + kGap;

  int y = kTop;
  bool first_block = true;
  for (const auto& block : blocks) {
    if (block.empty()) continue;
    if (!first_block) {
      canvas.dottedHLine(kX0, y + 3, x1 - kX0, 2);
      y += kSeparatorHeight;
    }
    first_block = false;
    for (const Line& l : block) {
      const int baseline = y + kBaseline;
      if (!l.label.empty()) {
        ui::drawText(canvas, l.sub ? subFont() : labelFont(), kX0, baseline, l.label);
      }
      if (l.entry != nullptr) {
        drawEntry(canvas, *l.entry, x_time, x_title, x1, baseline, language);
      } else if (l.more > 0) {
        const std::string text =
            "+" + std::to_string(l.more) + " " + ui::text(language, ui::Text::kMoreEvents);
        ui::drawText(canvas, subFont(), x_time, baseline, text);
      }
      y += kRowHeight;
    }
  }
}

}  // namespace widgets::calendar
