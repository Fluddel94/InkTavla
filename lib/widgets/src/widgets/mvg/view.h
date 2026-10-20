#pragma once

// View of the MVG widget: the departures in a column slot, either as one
// list or in freely named sections (e.g. "Stadt" / "Raus"), following the
// overall UI design (style A).

#include <cstdint>
#include <string>
#include <vector>

#include "ui/canvas.h"
#include "ui/i18n.h"
#include "widgets/mvg/departure.h"
#include "widgets/mvg/time_display.h"

namespace widgets::mvg {

struct ViewOptions {
  std::string title;        // station name, shown in capitals
  std::string note;         // right part of the tile title, "" for none
  bool note_alert = false;  // show the note as a badge (age hint for old data)
  TimeFormat time_format = TimeFormat::kClock;
  ui::Language language = ui::Language::kGerman;
  int64_t now_s = 0;  // current Unix time, 0 if unknown
  // MVV symbols (16 px bitmaps, drawn twice as large) for S-Bahn, U-Bahn and
  // tram instead of generated symbols. Off until the symbols are made in
  // their final size from the SVG sources (phase B).
  bool line_icons = false;
};

// Departures of one section. A single section without a name is drawn as
// one list.
struct Section {
  std::string name;
  std::vector<Departure> departures;
};

// Row heights (provisional, to be checked on the panel): 39 px in the list
// (10 departures), 38 px with sections (two sections: 9 departures).
inline constexpr int kListRowHeight = 39;
inline constexpr int kSectionRowHeight = 38;

// Number of rows that fit into a canvas of this height: as one list, or in
// total for the given number of named sections.
int listRowsThatFit(int canvas_height);
int sectionRowsThatFit(int canvas_height, int sections);

// Distributes `total` rows over sections with the given numbers of
// departures: evenly (earlier sections get the remainder), rows a section
// cannot fill go to the others. Every section keeps at least one row (for
// the "no departures" note).
std::vector<int> splitRows(int total, const std::vector<int>& available);

// Draws the tile title and as many departures as fit. Returns the number of
// departures drawn.
int drawDepartures(ui::Canvas& canvas, const std::vector<Section>& sections,
                   const ViewOptions& options);

}  // namespace widgets::mvg
