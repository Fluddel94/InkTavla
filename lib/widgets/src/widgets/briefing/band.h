#pragma once

// The band at the top of the "Briefing" layout (800 x 205): header line
// with date and time, the sentence (Fraunces, up to three lines) and the
// weather picture with the temperature of now ("8°") on the right, the
// rule below.
//
// The band always shows the clock; parts whose source has no fresh data are
// left out. A lost connection (after the grace period) is shown with a
// struck-through Wi-Fi symbol after the time and first in the sentence. The
// sentence reads the weather and the departures (see sentence.h) and names
// the widgets on the screen whose updates fail (weather, departures,
// calendar); the picture shows the weather of the band (see summarize()).

#include <string>
#include <vector>

#include "ui/canvas.h"
#include "ui/font.h"
#include "ui/i18n.h"
#include "ui/layout.h"
#include "widgets/briefing/sentence.h"
#include "widgets/mvg/widget.h"
#include "widgets/weather/widget.h"
#include "widgets/widget.h"

namespace widgets::briefing {

// Positions of the Briefing design (design/briefing/werkzeug/briefing.py),
// moved to the margin ui::kMargin (2026-10-25): the header line starts at
// the margin, its capitals begin at the margin from the top.
inline constexpr int kHeaderX = ui::kMargin;
inline constexpr int kHeaderCapHeight = 14;  // Roboto Condensed Bold 20
inline constexpr int kHeaderBaseline = ui::kMargin + kHeaderCapHeight;
inline constexpr int kSentenceX = ui::kMargin - 2;  // Fraunces has a small left bearing
// The weather picture keeps its distance of 150 px from the right margin;
// the sentence ends 32 px left of it.
inline constexpr int kPictureX = ui::kScreenWidth - ui::kMargin - 150;
inline constexpr int kSentenceWidth = kPictureX - 32 - kSentenceX;
inline constexpr int kPictureY = 18;
// "8°": centred under the drawn part of the picture, at most up to the
// right edge of the rule.
inline constexpr int kNowBaseline = 186;
inline constexpr int kRuleY = 204;

// Font of the header line (date and clock; also the heading of the Wi-Fi
// screen), in spaced capitals.
const ui::Font& headerFont();

// Lines of the sentence: Fraunces 38 with 44 px between baselines; if it
// does not fit into three lines, Fraunces 34 with 40 px; then shortened.
struct SentenceLayout {
  std::vector<std::string> lines;
  bool small = false;  // Fraunces 34
};
SentenceLayout layoutSentence(const std::string& sentence);

// The sentence from the greeting and the parts (ordered by rank) in three
// lines, in Fraunces 38 or else 34: the parts are taken by rank as long as
// the sentence fits; a part that does not fit is taken in its shorter form
// (the weather without the temperature), or the parts before it get their
// shorter forms ("Kein WLAN – Stand 07:12."), or else it is left out. The greeting with
// the most important part is shortened with "…" if necessary.
// *text (if given) is set to the sentence used.
SentenceLayout layoutParts(const std::string& greeting, const std::vector<Part>& parts,
                           std::string* text = nullptr);

// Data of the band; nullptr for a source without fresh data (or none).
struct BandData {
  // The forecast gives the picture; whether it is also in the sentence is
  // up to weather_sentence ("widgets.weather.band").
  const weather::Forecast* forecast = nullptr;
  bool weather_sentence = true;
  // The weather is not on the screen at all (not fetched, or no location
  // set): a placeholder takes the place of picture and temperature.
  bool weather_missing = false;
  std::string as_of;  // time of the last weather data ("" if unknown)
  // The departures give the cancellations of the sentence only if
  // departures_sentence is set ("widgets.mvg.band").
  const std::vector<mvg::Section>* departures = nullptr;
  bool departures_sentence = true;
  // Sources whose updates have failed for a while (stale or no data).
  std::vector<FailedSource> failed;
};

// The whole sentence before fitting: the greeting and all parts.
std::string bandSentence(const DrawContext& context, const BandData& data);

// Header line of the band: date and time in spaced capitals, the
// struck-through Wi-Fi symbol after the time when offline.
void drawHeaderLine(ui::Canvas& canvas, const DrawContext& context);

// Draws the band (see above) into a canvas of 800 x 205.
void drawBand(ui::Canvas& canvas, const DrawContext& context, const BandData& data);

// What the widgets add to the sentence ("widgets.<id>.band"). The notes on
// failed updates do not depend on it.
struct BandOptions {
  bool weather_sentence = true;
  bool departures_sentence = true;
};

class BandView : public View {
 public:
  // Any source may be nullptr (widget not on the screen or not available):
  // then its parts are left out. Without weather only the greeting and the
  // other parts, and a placeholder instead of the picture.
  // The calendar is only watched for failed updates.
  BandView(const weather::WeatherSource* weather, const mvg::MvgSource* departures,
           const DataSource* calendar = nullptr, BandOptions options = {})
      : weather_(weather), departures_(departures), calendar_(calendar), options_(options) {}

  std::vector<const DataSource*> sources() const override;
  bool handlesStates() const override { return true; }
  std::string title(ui::Language) const override { return ""; }
  void draw(ui::Canvas& canvas, const DrawContext& context) override;

 private:
  const weather::WeatherSource* weather_;
  const mvg::MvgSource* departures_;
  const DataSource* calendar_;
  BandOptions options_;
};

}  // namespace widgets::briefing
