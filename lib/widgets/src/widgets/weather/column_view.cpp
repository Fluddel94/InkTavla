#include "widgets/weather/column_view.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/time.h"
#include "ui/elements.h"
#include "ui/fonts.h"
#include "ui/icons.h"
#include "ui/text.h"

namespace widgets::weather {

namespace {

using ui::Color;

// Sizes of the Briefing design (design/briefing/werkzeug/weather3.py),
// relative to the column title.
constexpr int kBase = ui::kColumnTitleBaseline + 96;
constexpr int kCurveTop = ui::kColumnTitleBaseline + 32;  // room for the labels above
constexpr int kCurveGapAboveBars = 6;
constexpr int kCurveBottomWithoutRain = kBase - 12;
constexpr int kChartMargin = 6;
constexpr int kDaysTop = kBase + 34;
constexpr int kDayTemperatures = kDaysTop + 85;
constexpr int kLabelHours[] = {8, 11, 14, 17, 20};
constexpr double kCurveWidth = 2;
// Points of the spline between two hourly values.
constexpr int kStepsBetweenHours = 10;

int roundToInt(double v) { return static_cast<int>(std::lround(v)); }

std::string degreesText(float t) {
  char buffer[12];
  std::snprintf(buffer, sizeof(buffer), "%d°", displayTemperature(t));
  return buffer;
}

bool sameDay(const core::LocalTime& a, const core::LocalTime& b) {
  return a.year == b.year && a.month == b.month && a.day == b.day;
}

std::vector<ColumnChart::Point> smooth(const std::vector<ColumnChart::Point>& p, int steps) {
  if (p.size() < 2) return p;
  std::vector<ColumnChart::Point> out;
  for (size_t i = 0; i + 1 < p.size(); i++) {
    const auto& p0 = p[i == 0 ? 0 : i - 1];
    const auto& p1 = p[i];
    const auto& p2 = p[i + 1];
    const auto& p3 = p[std::min(i + 2, p.size() - 1)];
    for (int k = 0; k < steps; k++) {
      const double t = static_cast<double>(k) / steps;
      auto f = [&](double a, double b, double c, double d) {
        return 0.5 * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t * t +
                      (-a + 3 * b - 3 * c + d) * t * t * t);
      };
      out.push_back({f(p0.x, p1.x, p2.x, p3.x), f(p0.y, p1.y, p2.y, p3.y)});
    }
  }
  out.push_back(p.back());
  return out;
}

void drawChart(ui::Canvas& c, const ColumnChart& chart) {
  for (const ColumnChart::Bar& b : chart.bars) {
    c.fillDotted(b.x0, chart.base - b.height, b.x1 - b.x0 + 1, b.height);
  }
  c.hLine(chart.line_x0, chart.base, chart.line_x1 - chart.line_x0 + 1, Color::kBlack);
  c.drawPolyline(chart.curve, kCurveWidth, Color::kBlack);
  for (const ColumnChart::Label& l : chart.labels) {
    char t[12];
    std::snprintf(t, sizeof(t), "%d°", l.temperature);
    ui::drawText(c, ui::fonts::kRobotoCondensedBold16, l.x, l.y - 8, t, Color::kBlack,
                 ui::Align::kCenter);
    char h[4];
    std::snprintf(h, sizeof(h), "%d", l.hour);
    ui::drawText(c, ui::fonts::kRobotoCondensedMedium15, l.x, chart.base + 18, h, Color::kBlack,
                 ui::Align::kCenter);
  }
}

void drawDays(ui::Canvas& c, const Forecast& f, int64_t now_s, ui::Language lang) {
  const int width = c.width();
  c.dottedHLine(0, kDaysTop, width, 2);
  if (f.days.empty()) return;
  core::LocalTime today;
  core::toLocalTime(core::isValidUnixTime(now_s) ? now_s : f.days.front().time, today);
  std::vector<const Day*> days;
  for (const Day& d : f.days) {
    // Daily values start at midnight at the location; noon is safely inside.
    core::LocalTime t;
    core::toLocalTime(d.time + 12 * 3600, t);
    const bool later = t.year > today.year || (t.year == today.year && t.month > today.month) ||
                       (t.year == today.year && t.month == today.month && t.day > today.day);
    if (later && days.size() < 3) days.push_back(&d);
  }
  const double cw = width / 3.0;
  for (size_t i = 0; i < days.size(); i++) {
    const int cx = roundToInt(cw * i + cw / 2);
    if (i > 0) c.dottedVLine(roundToInt(cw * i), kDaysTop + 10, kDayTemperatures - kDaysTop - 8, 2);
    core::LocalTime t;
    core::toLocalTime(days[i]->time + 12 * 3600, t);
    ui::drawText(c, ui::fonts::kRobotoCondensedBold16, cx, kDaysTop + 24,
                 ui::weekdayShort(lang, t.weekday), Color::kBlack, ui::Align::kCenter);
    ui::drawIcon(c, cx - kColumnDayIconSize / 2, kDaysTop + 28,
                 ui::weatherIcon(dayIcon(days[i]->code, days[i]->condition), kColumnDayIconSize));
    // "10°/3°": high and low in bold (the low a little smaller), a thin
    // slash between them (2026-10-29: the low in Medium looked too thin).
    const std::string high = degreesText(days[i]->high);
    const std::string low = degreesText(days[i]->low);
    const ui::Font& hf = ui::fonts::kRobotoCondensedBold17;
    const ui::Font& sf = ui::fonts::kRobotoCondensedMedium16;
    const ui::Font& lf = ui::fonts::kRobotoCondensedBold16;
    const int x =
        cx - (ui::textWidth(hf, high) + ui::textWidth(sf, "/") + ui::textWidth(lf, low)) / 2;
    int end = ui::drawText(c, hf, x, kDayTemperatures, high);
    end = ui::drawText(c, sf, end, kDayTemperatures, "/");
    ui::drawText(c, lf, end, kDayTemperatures, low);
  }
}

}  // namespace

namespace {

// The JSON parser reads "0.10" as 0.0999999; a small tolerance keeps 0.1 mm
// a bar. !(>=) is also true for NaN.
bool wet(float mm) { return mm >= kRainMinMm - 0.001f; }

double share(double mm, double full) { return std::sqrt(std::min(1.0, mm / full)); }

}  // namespace

int rainBarHeight(float mm, float day_max) {
  if (!wet(mm)) return 0;
  if (!(day_max >= mm)) day_max = mm;
  double full = kRainFullMm;
  double stretch = 1;
  if (day_max > kRainFullMm) {
    full = std::min(day_max, kRainFullMaxMm);
  } else {
    stretch = std::clamp(kRainStretchTop / share(day_max, kRainFullMm), 1.0, kRainStretchMax);
  }
  const int h = roundToInt(share(mm, full) * stretch * kColumnRainMax);
  return std::clamp(h, kRainMinBar, kColumnRainMax);
}

ColumnChart columnChart(const Forecast& forecast, int64_t now_s, int width) {
  ColumnChart chart;
  chart.base = kBase;
  chart.curve_top = kCurveTop;
  // Hour marks a whole number of px apart, the chart centred in the column.
  constexpr int kHours = kLastHour - kFirstHour;
  const int pitch = std::max(3, (width - 2 * kChartMargin) / kHours);
  const int gx0 = (width - kHours * pitch) / 2;
  auto px = [&](double h) { return gx0 + (h - kFirstHour) * pitch; };
  chart.pitch = pitch;
  chart.line_x0 = gx0 + 1;                   // left edge of the bar of 6 h
  chart.line_x1 = gx0 + kHours * pitch - 2;  // right edge of the bar of 23 h
  if (forecast.hours.empty()) return chart;

  core::LocalTime today;
  core::toLocalTime(core::isValidUnixTime(now_s) ? now_s : forecast.hours.front().time, today);
  struct Item {
    int hour;
    const Hour* data;
  };
  std::vector<Item> items;
  for (const Hour& h : forecast.hours) {
    core::LocalTime t;
    if (!core::toLocalTime(h.time, t) || !sameDay(t, today)) continue;
    if (t.hour < kFirstHour || t.hour > kLastHour) continue;
    items.push_back({t.hour, &h});
  }
  if (items.size() < 2) return chart;

  // Bars first: the curve has to stay above the highest one. The value of
  // hour h is the precipitation from h - 1 to h: its bar fills that span.
  float day_max = 0;
  for (const Item& i : items) {
    if (i.hour > kFirstHour) day_max = std::max(day_max, i.data->precipitation);
  }
  int highest_bar = 0;
  for (const Item& i : items) {
    if (i.hour <= kFirstHour) continue;
    const int h = rainBarHeight(i.data->precipitation, day_max);
    if (h <= 0) continue;
    highest_bar = std::max(highest_bar, h);
    const int x0 = static_cast<int>(px(i.hour - 1)) + 1;
    chart.bars.push_back({x0, x0 + pitch - 3, h});
  }
  chart.curve_bottom =
      highest_bar > 0 ? std::min(kCurveBottomWithoutRain, kBase - highest_bar - kCurveGapAboveBars)
                      : kCurveBottomWithoutRain;

  // The temperatures of the curve: the hourly values, each averaged with its
  // neighbours (1-2-1), so that the curve runs smoothly (2026-10-29; the
  // labels keep the values of their hours).
  struct Value {
    double hour;
    float temperature;
  };
  std::vector<Value> values;
  for (size_t i = 0; i < items.size(); i++) {
    const float before = items[i == 0 ? 0 : i - 1].data->temperature;
    const float after = items[std::min(i + 1, items.size() - 1)].data->temperature;
    values.push_back({static_cast<double>(items[i].hour),
                      (before + 2 * items[i].data->temperature + after) / 4});
  }
  float tmin = values[0].temperature, tmax = tmin;
  for (const Value& v : values) {
    tmin = std::min(tmin, v.temperature);
    tmax = std::max(tmax, v.temperature);
  }
  const double span = std::max<double>(tmax - tmin, kColumnMinSpan);
  const double low = (tmin + tmax) / 2.0 - span / 2.0;
  auto py = [&](double t) {
    return chart.curve_bottom - (t - low) / span * (chart.curve_bottom - chart.curve_top);
  };

  std::vector<ColumnChart::Point> raw;
  for (const Value& v : values) raw.push_back({px(v.hour), py(v.temperature)});
  chart.curve = smooth(raw, kStepsBetweenHours);
  // The spline may swing a little beyond the points: keep it inside.
  for (ColumnChart::Point& p : chart.curve) {
    p.y = std::clamp<double>(p.y, chart.curve_top, chart.curve_bottom);
  }
  // Labels: the value of the hour, placed on the (averaged) curve.
  for (size_t i = 0; i < items.size(); i++) {
    const int hour = items[i].hour;
    if (std::find(std::begin(kLabelHours), std::end(kLabelHours), hour) == std::end(kLabelHours)) {
      continue;
    }
    chart.labels.push_back({hour, roundToInt(px(hour)), roundToInt(py(values[i].temperature)),
                            displayTemperature(items[i].data->temperature)});
  }
  return chart;
}

void drawWeatherColumn(ui::Canvas& canvas, const Forecast& forecast, const ColumnOptions& options) {
  const bool place = !options.place.empty();
  ui::drawColumnTitle(canvas, 0, canvas.width(), ui::kColumnTitleBaseline,
                      place ? options.place : ui::text(options.language, ui::Text::kTitleWeather),
                      options.note.empty() ? kAttribution : options.note,
                      place ? ui::TitleIcon::kPin : ui::TitleIcon::kNone, options.note_alert);
  drawChart(canvas, columnChart(forecast, options.now_s, canvas.width()));
  drawDays(canvas, forecast, options.now_s, options.language);
}

std::string ForecastColumnView::title(ui::Language language) const {
  const std::string& place = source_.config().name;
  return place.empty() ? std::string(ui::text(language, ui::Text::kTitleWeather)) : place;
}

void ForecastColumnView::drawTitle(ui::Canvas& canvas, ui::Language language) {
  ui::drawColumnTitle(canvas, 0, canvas.width(), ui::kColumnTitleBaseline, title(language), "",
                      source_.config().name.empty() ? ui::TitleIcon::kNone : ui::TitleIcon::kPin);
}

void ForecastColumnView::draw(ui::Canvas& canvas, const DrawContext& context) {
  ColumnOptions options;
  options.place = source_.config().name;
  // The age badge replaces the credit while the data is old.
  options.note = context.note;
  options.note_alert = context.note_alert;
  options.language = context.language;
  options.now_s = context.now_s;
  drawWeatherColumn(canvas, source_.forecast(), options);
}

}  // namespace widgets::weather
