#include "widgets/briefing/band.h"

#include <cstdio>

#include "core/time.h"
#include "ui/elements.h"
#include "ui/fonts.h"
#include "ui/icons.h"
#include "ui/layout.h"
#include "ui/text.h"
#include "widgets/briefing/sentence.h"

namespace widgets::briefing {

namespace {

using ui::Color;

constexpr int kHeaderSpacing = 16;  // 2 px
constexpr int kWifiSize = 18;

}  // namespace

SentenceLayout layoutSentence(const std::string& sentence) {
  SentenceLayout out;
  bool complete = true;
  out.lines = ui::wrapText(ui::fonts::kFraunces38, sentence, kSentenceWidth, 3, &complete);
  if (complete) return out;
  out.small = true;
  out.lines = ui::wrapText(ui::fonts::kFraunces34, sentence, kSentenceWidth, 3);
  return out;
}

std::vector<const DataSource*> BandView::sources() const {
  if (weather_ == nullptr) return {};
  return {weather_};
}

std::string bandSentence(const DrawContext& context, const weather::Forecast* forecast,
                         const std::string& as_of) {
  core::LocalTime now;
  if (!core::isValidUnixTime(context.now_s) || !core::toLocalTime(context.now_s, now)) return "";
  const Greeting greeting = greetingFor(now.hour);
  std::string text = greetingText(greeting, context.language);
  if (!context.network_connected) text += " " + offlineText(as_of, context.language);
  if (forecast != nullptr) {
    const std::string part =
        weatherText(summarize(*forecast, context.now_s), greeting, context.language);
    if (!part.empty()) text += " " + part;
  }
  return text;
}

void drawBand(ui::Canvas& canvas, const DrawContext& context, const weather::Forecast* forecast,
              const std::string& as_of) {
  // Header line: "DIENSTAG, 20. OKTOBER · 07:42" (Wi-Fi symbol when offline).
  core::LocalTime local;
  const bool valid =
      core::isValidUnixTime(context.now_s) && core::toLocalTime(context.now_s, local);
  char time[8] = "--:--";
  std::string header = time;
  if (valid) {
    std::snprintf(time, sizeof(time), "%02d:%02d", local.hour, local.minute);
    header = ui::toUpper(ui::longDate(context.language, local.weekday, local.day, local.month)) +
             " \u00b7 " + time;
  }
  const int end = ui::drawTrackedText(canvas, ui::fonts::kRobotoCondensedBold16, kHeaderX,
                                      kHeaderBaseline, header, kHeaderSpacing);
  if (!context.network_connected) {
    ui::drawWifiSymbol(canvas, end + 12, kHeaderBaseline - kWifiSize + 2, kWifiSize, false);
  }

  // Sentence.
  const SentenceLayout layout = layoutSentence(bandSentence(context, forecast, as_of));
  const ui::Font& font = layout.small ? ui::fonts::kFraunces34 : ui::fonts::kFraunces38;
  const int first = layout.small ? 80 : 82;
  const int step = layout.small ? 40 : 44;
  for (size_t i = 0; i < layout.lines.size(); i++) {
    ui::drawText(canvas, font, kSentenceX, first + step * static_cast<int>(i), layout.lines[i]);
  }

  // Weather picture with today's low and high.
  if (forecast != nullptr) {
    const WeatherSummary s = summarize(*forecast, context.now_s);
    if (s.valid) {
      ui::drawIcon(canvas, kPictureX, kPictureY, ui::bandIcon(s.picture));
      ui::drawText(canvas, ui::fonts::kFraunces24Medium, kRangeX, kRangeBaseline,
                   rangeText(s, context.language));
    }
  }

  canvas.hLine(kHeaderX, kRuleY, ui::kScreenWidth - 2 * kHeaderX, Color::kBlack);
}

void BandView::draw(ui::Canvas& canvas, const DrawContext& context) {
  // Only fresh weather data goes into the band; old data is left out.
  const bool fresh = weather_ != nullptr && !context.sources.empty() &&
                     context.sources[0].state == core::DataState::kFresh;
  std::string as_of;
  if (weather_ != nullptr && !context.sources.empty() &&
      core::isValidUnixTime(context.sources[0].last_success_wall_s)) {
    core::LocalTime t;
    core::toLocalTime(context.sources[0].last_success_wall_s, t);
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", t.hour, t.minute);
    as_of = buffer;
  }
  drawBand(canvas, context, fresh ? &weather_->forecast() : nullptr, as_of);
}

}  // namespace widgets::briefing
