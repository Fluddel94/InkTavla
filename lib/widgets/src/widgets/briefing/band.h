#pragma once

// The band at the top of the "Briefing" layout (800 x 205): header line
// with date and time, the sentence (Fraunces, up to three lines) and the
// weather picture with today's low and high on the right, the rule below.
//
// The band always shows the clock; parts whose source has no fresh data are
// left out. A lost connection (after the grace period) is shown with a
// struck-through Wi-Fi symbol after the time and first in the sentence.

#include <string>
#include <vector>

#include "ui/canvas.h"
#include "ui/i18n.h"
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
inline constexpr int kRangeX = 630;
inline constexpr int kRangeBaseline = 186;
inline constexpr int kRuleY = 204;

// Lines of the sentence: Fraunces 38 with 44 px between baselines; if it
// does not fit into three lines, Fraunces 34 with 40 px; then shortened.
struct SentenceLayout {
  std::vector<std::string> lines;
  bool small = false;  // Fraunces 34
};
SentenceLayout layoutSentence(const std::string& sentence);

// The sentence: greeting, the offline note (with the time of the last
// weather data, "" if unknown) and the weather part; forecast = nullptr if
// there is no fresh weather data.
std::string bandSentence(const DrawContext& context, const weather::Forecast* forecast,
                         const std::string& as_of);

// Draws the band (see above) into a canvas of 800 x 205.
void drawBand(ui::Canvas& canvas, const DrawContext& context, const weather::Forecast* forecast,
              const std::string& as_of);

class BandView : public View {
 public:
  // weather may be nullptr (no weather configured): then only the greeting.
  explicit BandView(const weather::WeatherSource* weather) : weather_(weather) {}

  std::vector<const DataSource*> sources() const override;
  bool handlesStates() const override { return true; }
  std::string title(ui::Language) const override { return ""; }
  void draw(ui::Canvas& canvas, const DrawContext& context) override;

 private:
  const weather::WeatherSource* weather_;
};

}  // namespace widgets::briefing
