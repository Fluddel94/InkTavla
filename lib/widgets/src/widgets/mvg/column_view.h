#pragma once

// View of the MVG widget in a column of the "Briefing" layout (2026-10-21):
// no frame, the sections (or one list) fill the column down to its bottom
// edge; rows of 28 px with line symbol, destination, planned time and delay.
// Disruption messages of the shown lines (2026-10-26): below the station
// title, a warning sign and the wrapped text with the lines in front ("S2:
// Verspätung eines vorausfahrenden Zuges."), no heading.

#include <string>
#include <vector>

#include "ui/canvas.h"
#include "ui/i18n.h"
#include "widgets/mvg/departure.h"
#include "widgets/mvg/time_display.h"
#include "widgets/mvg/widget.h"

namespace widgets::mvg {

struct ColumnOptions {
  // Station title with the location pin: above one list and above two
  // directions (not above two stations, their titles have the pin).
  std::string station;
  std::string station_note;  // free text on the right of it ("Bahnhof"), "" = none
  // Age badge of the first title, "" for none; it takes the place of the
  // note.
  std::string note;
  bool note_alert = false;
  TimeFormat time_format = TimeFormat::kClock;
  ui::Language language = ui::Language::kGerman;
  int64_t now_s = 0;
  // Disruption messages shown above the sections (Briefing design D).
  std::vector<Incident> incidents;
};

// Text of the disruption messages, every message ending with a full stop.
// If all messages have the same lines, they are named once in front ("S2:
// Reparatur an einem Signal. Verspätung eines vorausfahrenden Zuges."),
// otherwise in front of every message ("S6: Technische Störung am Zug. S4,
// S6, S8: Verspätung eines vorausfahrenden Zuges.").
std::string incidentText(const std::vector<Incident>& incidents);

// Most lines of the disruption text above a list and above two sections;
// fewer (at least one) if otherwise a section would get fewer than
// kIncidentMinRows rows.
inline constexpr int kIncidentMaxLines = 3;
inline constexpr int kIncidentMaxLinesSections = 2;
inline constexpr int kIncidentMinRows = 2;

// Row height and the space between the last row of a section and the next
// section title (Briefing design).
inline constexpr int kColumnRowHeight = 28;
inline constexpr int kColumnSectionGap = 22;

// Rows that fit into a column of this height for the given number of
// sections (1 = one list).
int columnRowsThatFit(int canvas_height, int sections);

// Space between the station title and the first section title below it.
inline constexpr int kStationTitleStep = kColumnSectionGap + 6;

// Draws the disruption messages (if any), the station title and the
// departures (one unnamed section = one list). Returns the number of
// departures drawn.
int drawDeparturesColumn(ui::Canvas& canvas, const std::vector<Section>& sections,
                         const ColumnOptions& options);

class DeparturesColumnView : public View {
 public:
  // source must outlive the view.
  explicit DeparturesColumnView(const MvgSource& source) : source_(source) {}

  std::vector<const DataSource*> sources() const override { return {&source_}; }
  std::string title(ui::Language language) const override;
  void draw(ui::Canvas& canvas, const DrawContext& context) override;
  void drawTitle(ui::Canvas& canvas, ui::Language language) override;

 private:
  const MvgSource& source_;
};

}  // namespace widgets::mvg
