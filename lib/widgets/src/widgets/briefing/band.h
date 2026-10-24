#pragma once

// The band at the top of the "Briefing" layout (800 x 205): header line
// with date and time, the sentence (Fraunces, up to three lines) and the
// weather picture with today's low and high on the right, the rule below.
//
// The band always shows the clock; parts whose source has no fresh data are
// left out. A lost connection (after the grace period) is shown with a
// struck-through Wi-Fi symbol after the time and first in the sentence. The
// sentence reads the weather, the departures and the calendar (see
// sentence.h); the picture always shows the weather.

#include <string>
#include <vector>

#include "ui/canvas.h"
#include "ui/i18n.h"
#include "widgets/briefing/sentence.h"
#include "widgets/calendar/widget.h"
#include "widgets/mvg/widget.h"
#include "widgets/weather/widget.h"
#include "widgets/widget.h"

namespace widgets::briefing {

// Positions of the Briefing design (design/briefing/werkzeug/briefing.py).
inline constexpr int kHeaderX = 28;
inline constexpr int kHeaderBaseline = 34;
inline constexpr int kSentenceX = 26;
inline constexpr int kSentenceWidth = 564;  // up to x 590, left of the picture
inline constexpr int kPictureX = 622;
inline constexpr int kPictureY = 18;
// Low and high: centred under the drawn part of the picture, at most up to
// the right edge of the rule.
inline constexpr int kRangeBaseline = 186;
inline constexpr int kRuleY = 204;

// Lines of the sentence: Fraunces 38 with 44 px between baselines; if it
// does not fit into three lines, Fraunces 34 with 40 px; then shortened.
struct SentenceLayout {
  std::vector<std::string> lines;
  bool small = false;  // Fraunces 34
};
SentenceLayout layoutSentence(const std::string& sentence);

// The sentence from the greeting and the parts (ordered by rank) in three
// lines, in Fraunces 38 or else 34: the parts are taken by rank as long as
// the sentence fits; a part that does not fit is left out, a shorter one of
// a lower rank may still be taken. The greeting with the most important part
// is shortened with "…" if necessary.
// *text (if given) is set to the sentence used.
SentenceLayout layoutParts(const std::string& greeting, const std::vector<Part>& parts,
                           std::string* text = nullptr);

// Data of the band; nullptr for a source without fresh data (or none).
struct BandData {
  const weather::Forecast* forecast = nullptr;
  std::string as_of;  // time of the last weather data ("" if unknown)
  const std::vector<mvg::Section>* departures = nullptr;
  const std::vector<mvg::Incident>* incidents = nullptr;
  const calendar::Occurrences* events = nullptr;
};

// The whole sentence before fitting: the greeting and all parts.
std::string bandSentence(const DrawContext& context, const BandData& data);

// Draws the band (see above) into a canvas of 800 x 205.
void drawBand(ui::Canvas& canvas, const DrawContext& context, const BandData& data);

class BandView : public View {
 public:
  // Any source may be nullptr (widget not on the screen or not available):
  // then its parts are left out. Without weather only the greeting and the
  // other parts.
  BandView(const weather::WeatherSource* weather, const mvg::MvgSource* departures,
           const calendar::CalendarSource* calendar)
      : weather_(weather), departures_(departures), calendar_(calendar) {}

  std::vector<const DataSource*> sources() const override;
  bool handlesStates() const override { return true; }
  std::string title(ui::Language) const override { return ""; }
  void draw(ui::Canvas& canvas, const DrawContext& context) override;

 private:
  const weather::WeatherSource* weather_;
  const mvg::MvgSource* departures_;
  const calendar::CalendarSource* calendar_;
};

}  // namespace widgets::briefing
