#pragma once

// View of the MVG widget in a column of the "Briefing" layout (2026-10-21):
// no frame, the sections (or one list) fill the column down to its bottom
// edge; rows of 28 px with line symbol, destination, planned time and delay.
// Disruption messages of the shown lines come first, with a warning sign, a
// heading and a wrapped text.

#include <string>
#include <vector>

#include "ui/canvas.h"
#include "ui/i18n.h"
#include "widgets/mvg/departure.h"
#include "widgets/mvg/time_display.h"
#include "widgets/mvg/widget.h"

namespace widgets::mvg {

struct ColumnOptions {
  std::string station;  // title of a single list (with the location pin)
  std::string note;     // age badge of the first title, "" for none
  bool note_alert = false;
  TimeFormat time_format = TimeFormat::kClock;
  ui::Language language = ui::Language::kGerman;
  int64_t now_s = 0;
  // Disruption messages shown above the sections (Briefing design D).
  std::vector<Incident> incidents;
};

// Heading of the disruption messages: "Störung S3, S4" / "Disruption S3, S4".
std::string incidentHeading(const std::vector<Incident>& incidents, ui::Language language);

// Text below the heading: one message as it is ("Verspätung eines
// vorausfahrenden Zuges."); several each with their lines ("S3, S4:
// Verspätung eines vorausfahrenden Zuges. S6: Technische Störung am Zug.").
std::string incidentText(const std::vector<Incident>& incidents);

// Most lines of the disruption text above a list and above two sections;
// fewer if otherwise a section would get no row.
inline constexpr int kIncidentMaxLines = 3;
inline constexpr int kIncidentMaxLinesSections = 2;

// Row height and the space between the last row of a section and the next
// section title (Briefing design).
inline constexpr int kColumnRowHeight = 28;
inline constexpr int kColumnSectionGap = 22;

// Rows that fit into a column of this height for the given number of
// sections (1 = one list).
int columnRowsThatFit(int canvas_height, int sections);

// Draws the disruption messages (if any) and the departures (one unnamed
// section = one list with the station as title). Returns the number of
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
