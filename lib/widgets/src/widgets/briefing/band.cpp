#include "widgets/briefing/band.h"

#include <algorithm>
#include <cstdio>
#include <utility>

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

// Centre (x) of the drawn pixels of an icon, relative to its left edge.
int inkCentre(const ui::Icon& icon) {
  const int bytes_per_row = (icon.width + 7) / 8;
  int left = icon.width;
  int right = -1;
  for (int y = 0; y < icon.height; y++) {
    for (int x = 0; x < icon.width; x++) {
      if ((icon.bits[y * bytes_per_row + x / 8] >> (7 - x % 8)) & 1) {
        left = std::min(left, x);
        right = std::max(right, x);
      }
    }
  }
  return right < 0 ? icon.width / 2 : (left + right + 1) / 2;
}

}  // namespace

const ui::Font& headerFont() { return ui::fonts::kRobotoCondensedBold20; }

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
  // left out (a shorter one of a lower rank may still fit). Before a part is
  // left out, the parts taken before it get their shorter forms if that
  // makes room ("Kein WLAN – Stand 07:41." instead of the long notice).
  std::vector<Part> kept;
  for (const Part& part : parts) {
    kept.push_back(part);
    if (fits(joinSentence(greeting, kept), nullptr) || kept.size() == 1) continue;
    if (!part.shorter.empty()) {
      kept.back().text = part.shorter;
      if (fits(joinSentence(greeting, kept), nullptr)) continue;
    }
    std::vector<Part> shortened = kept;
    for (size_t i = 0; i + 1 < shortened.size(); i++) {
      if (!shortened[i].shorter.empty()) shortened[i].text = shortened[i].shorter;
    }
    shortened.back().text = part.text;
    bool taken = fits(joinSentence(greeting, shortened), nullptr);
    if (!taken && !part.shorter.empty()) {
      shortened.back().text = part.shorter;
      taken = fits(joinSentence(greeting, shortened), nullptr);
    }
    if (taken) {
      kept = shortened;
      continue;
    }
    kept.pop_back();
  }
  const std::string sentence = joinSentence(greeting, kept);
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
  in.failed = data.failed;
  return in;
}

std::string greetingNow(const DrawContext& context) {
  core::LocalTime now;
  if (!core::isValidUnixTime(context.now_s) || !core::toLocalTime(context.now_s, now)) return "";
  return greetingText(greetingFor(now.hour), context.language);
}

}  // namespace

std::string bandSentence(const DrawContext& context, const BandData& data) {
  return joinSentence(greetingNow(context), sentenceParts(sentenceInput(context, data)));
}

void drawHeaderLine(ui::Canvas& canvas, const DrawContext& context) {
  // "DIENSTAG, 20. OKTOBER · 07:42" (Wi-Fi symbol when offline).
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
  const int end =
      ui::drawTrackedText(canvas, headerFont(), kHeaderX, kHeaderBaseline, header, kHeaderSpacing);
  if (!context.network_connected) {
    ui::drawWifiSymbol(canvas, end + 12, kHeaderBaseline - kWifiSize + 2, kWifiSize, false);
  }
}

void drawBand(ui::Canvas& canvas, const DrawContext& context, const BandData& data) {
  drawHeaderLine(canvas, context);

  // Sentence.
  const SentenceLayout layout =
      layoutParts(greetingNow(context), sentenceParts(sentenceInput(context, data)));
  const ui::Font& font = layout.small ? ui::fonts::kFraunces34 : ui::fonts::kFraunces38;
  const int first = layout.small ? 80 : 82;
  const int step = layout.small ? 40 : 44;
  for (size_t i = 0; i < layout.lines.size(); i++) {
    ui::drawText(canvas, font, kSentenceX, first + step * static_cast<int>(i), layout.lines[i]);
  }

  // Weather picture with the temperature of now.
  if (data.forecast != nullptr) {
    const WeatherSummary s = summarize(*data.forecast, context.now_s);
    if (s.valid) {
      const ui::Icon& picture = ui::bandIcon(s.picture);
      ui::drawIcon(canvas, kPictureX, kPictureY, picture);
      const ui::Font& font = ui::fonts::kFraunces38;  // as the sentence (2026-10-29)
      const std::string now = nowText(s, context.language);
      const int width = ui::textWidth(font, now);
      const int x =
          std::min(kPictureX + inkCentre(picture) - width / 2, ui::kScreenWidth - kHeaderX - width);
      ui::drawText(canvas, font, x, kNowBaseline, now);
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
  // Failed updates (stale data, or none after the grace period).
  const std::pair<const DataSource*, Source> watched[] = {{weather_, Source::kWeather},
                                                          {departures_, Source::kDepartures},
                                                          {calendar_, Source::kCalendar}};
  for (const auto& [source, kind] : watched) {
    const SourceStatus* s = source == nullptr ? nullptr : status(source);
    if (s == nullptr) continue;
    if (s->state != core::DataState::kStale && s->state != core::DataState::kNoData) continue;
    FailedSource failed{kind, ""};
    if (s->state == core::DataState::kStale && core::isValidUnixTime(s->last_success_wall_s)) {
      core::LocalTime t;
      core::toLocalTime(s->last_success_wall_s, t);
      char buffer[8];
      std::snprintf(buffer, sizeof(buffer), "%02d:%02d", t.hour, t.minute);
      failed.as_of = buffer;
    }
    data.failed.push_back(failed);
  }
  if (fresh(weather_)) data.forecast = &weather_->forecast();
  if (fresh(departures_)) data.departures = &departures_->sections();
  drawBand(canvas, context, data);
}

}  // namespace widgets::briefing
