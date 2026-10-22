#include "widgets/briefing/band.h"

#include <cstdio>

#include "core/time.h"
#include "ui/elements.h"
#include "ui/fonts.h"
#include "ui/icons.h"
#include "ui/layout.h"
#include "ui/text.h"

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

SentenceLayout layoutParts(const std::string& greeting, const std::vector<Part>& parts,
                           std::string* text) {
  // Fits the sentence into three lines (Fraunces 38, else 34)?
  const auto fits = [](const std::string& sentence, SentenceLayout* out) {
    bool complete = true;
    SentenceLayout l;
    l.lines = ui::wrapText(ui::fonts::kFraunces38, sentence, kSentenceWidth, 3, &complete);
    if (!complete) {
      l.small = true;
      l.lines = ui::wrapText(ui::fonts::kFraunces34, sentence, kSentenceWidth, 3, &complete);
    }
    if (out != nullptr) *out = l;
    return complete;
  };
  // By rank: every part that still fits is taken, a part that does not is
  // left out (a shorter one of a lower rank may still fit).
  std::vector<Part> kept;
  for (const Part& part : parts) {
    kept.push_back(part);
    if (!fits(joinSentence(greeting, kept, kept.size()), nullptr) && kept.size() > 1) {
      kept.pop_back();
    }
  }
  const std::string sentence = joinSentence(greeting, kept, kept.size());
  if (text != nullptr) *text = sentence;
  SentenceLayout out;
  fits(sentence, &out);  // the most important part alone is shortened if needed
  return out;
}

std::vector<const DataSource*> BandView::sources() const {
  std::vector<const DataSource*> list;
  if (weather_ != nullptr) list.push_back(weather_);
  if (departures_ != nullptr) list.push_back(departures_);
  if (calendar_ != nullptr) list.push_back(calendar_);
  return list;
}

namespace {

SentenceInput sentenceInput(const DrawContext& context, const BandData& data) {
  SentenceInput in;
  in.now_s = context.now_s;
  in.language = context.language;
  in.online = context.network_connected;
  in.as_of = data.as_of;
  in.forecast = data.forecast;
  in.departures = data.departures;
  in.incidents = data.incidents;
  in.events = data.events;
  return in;
}

std::string greetingNow(const DrawContext& context) {
  core::LocalTime now;
  if (!core::isValidUnixTime(context.now_s) || !core::toLocalTime(context.now_s, now)) return "";
  return greetingText(greetingFor(now.hour), context.language);
}

}  // namespace

std::string bandSentence(const DrawContext& context, const BandData& data) {
  const std::vector<Part> parts = sentenceParts(sentenceInput(context, data));
  return joinSentence(greetingNow(context), parts, parts.size());
}

void drawBand(ui::Canvas& canvas, const DrawContext& context, const BandData& data) {
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
  const SentenceLayout layout =
      layoutParts(greetingNow(context), sentenceParts(sentenceInput(context, data)));
  const ui::Font& font = layout.small ? ui::fonts::kFraunces34 : ui::fonts::kFraunces38;
  const int first = layout.small ? 80 : 82;
  const int step = layout.small ? 40 : 44;
  for (size_t i = 0; i < layout.lines.size(); i++) {
    ui::drawText(canvas, font, kSentenceX, first + step * static_cast<int>(i), layout.lines[i]);
  }

  // Weather picture with today's low and high.
  if (data.forecast != nullptr) {
    const WeatherSummary s = summarize(*data.forecast, context.now_s);
    if (s.valid) {
      ui::drawIcon(canvas, kPictureX, kPictureY, ui::bandIcon(s.picture));
      ui::drawText(canvas, ui::fonts::kFraunces24Medium, kRangeX, kRangeBaseline,
                   rangeText(s, context.language));
    }
  }

  canvas.hLine(kHeaderX, kRuleY, ui::kScreenWidth - 2 * kHeaderX, Color::kBlack);
}

void BandView::draw(ui::Canvas& canvas, const DrawContext& context) {
  // Only fresh data goes into the band; old data is left out.
  const std::vector<const DataSource*> list = sources();
  const auto status = [&](const DataSource* source) -> const SourceStatus* {
    for (size_t i = 0; i < list.size() && i < context.sources.size(); i++) {
      if (list[i] == source) return &context.sources[i];
    }
    return nullptr;
  };
  const auto fresh = [&](const DataSource* source) {
    const SourceStatus* s = source == nullptr ? nullptr : status(source);
    return s != nullptr && s->state == core::DataState::kFresh;
  };
  BandData data;
  const SourceStatus* weather = weather_ == nullptr ? nullptr : status(weather_);
  if (weather != nullptr && core::isValidUnixTime(weather->last_success_wall_s)) {
    core::LocalTime t;
    core::toLocalTime(weather->last_success_wall_s, t);
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", t.hour, t.minute);
    data.as_of = buffer;
  }
  if (fresh(weather_)) data.forecast = &weather_->forecast();
  if (fresh(departures_)) {
    data.departures = &departures_->sections();
    data.incidents = &departures_->incidents();
  }
  if (fresh(calendar_)) data.events = &calendar_->occurrences();
  drawBand(canvas, context, data);
}

}  // namespace widgets::briefing
