#pragma once

// View of the MVG widget in a column of the "Briefing" layout (2026-10-21):
// no frame, the sections (or one list) fill the column down to its bottom
// edge; rows of 28 px with line symbol, destination, planned time and delay.
// Disruption messages of the shown departures (2026-10-26): below the
// station title (two stations: below the title of the first one), a warning
// sign and the wrapped, hyphenated text, no heading. Only the first message
// is shown, with "+2 weitere" for the others (2026-10-28). On step L the
// delay has its unit, "+8 min" (2026-11-16).

#include <string>
#include <vector>

#include "ui/canvas.h"
#include "ui/i18n.h"
#include "ui/layout.h"
#include "widgets/mvg/departure.h"
#include "widgets/mvg/time_display.h"
#include "widgets/mvg/widget.h"

namespace widgets::mvg {

struct ColumnOptions {
  // Station title with the location pin: above one list and above two
  // directions (not above two stations, their titles have the pin).
  std::string station;
  // Age badge of the first title ("Stand 14:32"), "" for none.
  std::string note;
  bool note_alert = false;
  TimeFormat time_format = TimeFormat::kClock;
  ui::Language language = ui::Language::kGerman;
  int64_t now_s = 0;
  // Most lines of the disruption text of the departures drawn, below the
  // station title (Briefing design D). 0: only the warning signs in the rows.
  int incident_lines = 0;
  // Step L (2026-11-16): the delay with its unit ("+8 min").
  bool delay_unit = false;
};

// The disruption text: only the first message (the one of the earliest
// departure drawn), ending with a full stop, with its lines in front ("S2:
// Reparatur an einem Signal.") if with_lines; the lines are left out when the
// station or section shows only one line. If there are more messages, `more`
// says so ("+2 weitere"); it is put at the end of the text.
struct IncidentLabel {
  std::string text;
  std::string more;
};
IncidentLabel incidentLabel(const std::vector<Incident>& incidents, bool with_lines,
                            ui::Language language);

// The disruption text gets fewer lines than allowed (at least one) if
// otherwise a section would get fewer than kIncidentMinRows rows.
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
// departures drawn. The messages are those of the departures drawn, not of
// all that were fetched; the text lines drawn are copied to incident_lines
// if given (tests).
int drawDeparturesColumn(ui::Canvas& canvas, const std::vector<Section>& sections,
                         const ColumnOptions& options,
                         std::vector<std::string>* incident_lines = nullptr);

class DeparturesColumnView : public View {
 public:
  // source must outlive the view.
  DeparturesColumnView(const MvgSource& source, ui::SizeStep step = ui::SizeStep::kM)
      : source_(source), step_(step) {}

  std::vector<const DataSource*> sources() const override { return {&source_}; }
  std::string title(ui::Language language) const override;
  void draw(ui::Canvas& canvas, const DrawContext& context) override;
  void drawTitle(ui::Canvas& canvas, ui::Language language) override;

 private:
  const MvgSource& source_;
  ui::SizeStep step_;
};

}  // namespace widgets::mvg
