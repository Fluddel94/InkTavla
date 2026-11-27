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

// The picture of the weather as a dotted outline (5 px grid, 3 px dots).
void drawDottedIcon(ui::Canvas& canvas, int x, int y, const ui::Icon& icon) {
  const int stride = (icon.width + 7) / 8;
  for (int row = 0; row < icon.height; row++) {
    for (int col = 0; col < icon.width; col++) {
      if (!(icon.bits[row * stride + col / 8] & (0x80 >> (col % 8)))) continue;
      if (col % 5 < 3 && row % 5 < 3) canvas.setPixel(x + col, y + row, Color::kBlack);
    }
  }
}

}  // namespace

const ui::Font& headerFont() { return ui::fonts::kRobotoCondensedBold20; }

int sentenceFill(const std::string& sentence) {
  return ui::textWidth(ui::fonts::kFraunces38, sentence) * 100 / (kSentenceLines * kSentenceWidth);
}

SentenceLayout layoutParts(const std::string& greeting, const std::vector<Part>& parts,
                           std::string* text) {
  const ui::Font& font = ui::fonts::kFraunces38;
  const int width_min = kSentenceLines * kSentenceWidth * kSentenceFillMinPercent / 100;
  const int width_max = kSentenceLines * kSentenceWidth * kSentenceFillMaxPercent / 100;
  std::vector<std::vector<std::string>> forms;
  // The same without the shortest forms (a title cut with "…"): those are
  // only used if nothing fits without them.
  std::vector<std::vector<std::string>> uncut;
  for (const Part& part : parts) {
    forms.push_back(partForms(part));
    uncut.push_back(forms.back());
    if (!part.shortest.empty() && uncut.back().size() > 1 && uncut.back().back() == part.shortest) {
      uncut.back().pop_back();
    }
  }

  // The best combination of the forms of the first `count` parts with or
  // without the greeting; false if none fits into three lines.
  const auto best = [&](const std::vector<std::vector<std::string>>& choices, size_t count,
                        const std::string& start, std::string* out) {
    std::vector<size_t> level(count, 0);  // 0 = longest form
    bool found = false;
    int best_distance = 0;
    std::vector<size_t> best_level;
    while (true) {
      std::string sentence = start;
      for (size_t i = 0; i < count; i++) {
        if (!sentence.empty()) sentence += " ";
        sentence += choices[i][level[i]];
      }
      bool complete = true;
      ui::wrapText(font, sentence, kSentenceWidth, kSentenceLines, &complete);
      if (complete) {
        const int w = ui::textWidth(font, sentence);
        // "At least half" counts more than the upper limit: a sentence below
        // half only wins if no other one reaches half. Within the range the
        // rank decides (below).
        const int distance = w < width_min   ? kSentenceLines * kSentenceWidth + width_min - w
                             : w > width_max ? w - width_max
                                             : 0;
        // Lexicographic: the longer forms of the higher ranks win a tie.
        if (!found || distance < best_distance ||
            (distance == best_distance && level < best_level)) {
          found = true;
          best_distance = distance;
          best_level = level;
          *out = sentence;
        }
      }
      // Next combination (the last part counts fastest).
      size_t i = count;
      while (i > 0) {
        i--;
        if (++level[i] < choices[i].size()) break;
        level[i] = 0;
        if (i == 0) return found;
      }
    }
  };

  std::string sentence;
  bool done = false;
  for (size_t count = parts.size(); count > 0 && !done; count--) {
    done = best(uncut, count, greeting, &sentence) || best(forms, count, greeting, &sentence) ||
           (!greeting.empty() && best(forms, count, "", &sentence));
  }
  if (!done) {
    // Nothing fits: the most important part in its shortest form, shortened
    // with "…"; the greeting goes first, as everywhere (CR2b N12). Without
    // parts the greeting alone.
    sentence = forms.empty() ? greeting : forms[0].back();
  }
  if (text != nullptr) *text = sentence;
  SentenceLayout out;
  out.lines = ui::wrapText(font, sentence, kSentenceWidth, kSentenceLines);
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
  in.forecast = data.weather_sentence ? data.forecast : nullptr;
  in.departures = data.departures_sentence ? data.departures : nullptr;
  in.events = data.event_sentence ? data.events : nullptr;
  in.event_display = data.event_display;
  in.failed = data.failed;
  return in;
}

// The greeting; alone (nothing else to say) the longer one with the weekday.
std::string greetingNow(const DrawContext& context, bool alone) {
  core::LocalTime now;
  if (!core::isValidUnixTime(context.now_s) || !core::toLocalTime(context.now_s, now)) return "";
  const Greeting greeting = greetingFor(now.hour);
  return alone ? greetingAlone(greeting, now.weekday, context.language)
               : greetingText(greeting, context.language);
}

}  // namespace

std::string bandSentence(const DrawContext& context, const BandData& data) {
  const std::vector<Part> parts = sentenceParts(sentenceInput(context, data));
  return joinSentence(greetingNow(context, parts.empty()), parts);
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
  const std::vector<Part> parts = sentenceParts(sentenceInput(context, data));
  const SentenceLayout layout = layoutParts(greetingNow(context, parts.empty()), parts);
  const ui::Font& font = ui::fonts::kFraunces38;
  const int first = 82;
  const int step = 44;
  for (size_t i = 0; i < layout.lines.size(); i++) {
    ui::drawText(canvas, font, kSentenceX, first + step * static_cast<int>(i), layout.lines[i]);
  }

  // Weather picture with the temperature of now.
  bool picture_drawn = false;
  if (data.forecast != nullptr) {
    const WeatherSummary s = summarize(*data.forecast, context.now_s);
    if (s.valid) {
      picture_drawn = true;
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

  // Placeholder for the picture and the temperature: no weather set up, or
  // no fresh data (old data stays out of the band, CR2b M6).
  if (!picture_drawn) {
    const ui::Icon& picture = ui::bandIcon(ui::WeatherIcon::kCloudy);
    drawDottedIcon(canvas, kPictureX, kPictureY, picture);
    const ui::Font& font = ui::fonts::kFraunces38;
    const std::string none = "\u2013\u00b0";
    const int width = ui::textWidth(font, none);
    ui::drawText(canvas, font, kPictureX + inkCentre(picture) - width / 2, kNowBaseline, none);
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
    const bool settings = source->problem(context.language).settings;
    const bool waiting_with_problem = s->state == core::DataState::kWaiting && settings;
    if (s->state != core::DataState::kStale && s->state != core::DataState::kNoData &&
        !waiting_with_problem) {
      continue;
    }
    FailedSource failed{kind, "", settings};
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
  if (fresh(calendar_)) {
    data.events = &calendar_->occurrences();
    data.event_display = calendar_->config().display;
  }
  data.event_sentence = options_.event_sentence;
  data.weather_sentence = options_.weather_sentence;
  data.departures_sentence = options_.departures_sentence;
  data.weather_missing = weather_ == nullptr || !weather_->isSetUp();
  drawBand(canvas, context, data);
}

}  // namespace widgets::briefing
