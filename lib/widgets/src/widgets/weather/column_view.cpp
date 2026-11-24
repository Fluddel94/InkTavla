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
constexpr int kChartMargin = 6;
constexpr int kDaysTop = ui::kColumnTitleBaseline + 130;
// W3 (2026-11-24): the curve reaches down to 8 px above the dotted line of
// the days; above its highest point there is room for symbol (30 px, 4 px
// gap), temperature (12 px) and 8 px to the point, 6 px below the title.
constexpr int kCurveTop = ui::kColumnTitleBaseline + 6 + kChartIconSize + 4 + 12 + 8;
constexpr int kCurveBottom = kDaysTop - 8;
// Labels keep their centre this far from the edges of the column.
constexpr int kLabelEdge = 15;
// The three days below the dotted line, spread evenly down to the bottom of
// the column (2026-10-31, variant B of the probe): weekday, 44 px symbol and
// temperatures with gaps of about 11 px; offsets from kDaysTop.
struct DaysLayout {
  int icon_size, weekday_baseline, icon_top, temperatures_baseline;
};
constexpr DaysLayout kDays{kColumnDayIconSize, 23, 34, 100};
constexpr int kLabelHoursFour[] = {5, 10, 15, 20};
constexpr int kLabelHoursFive[] = {6, 10, 14, 18, 22};
constexpr double kCurveWidth = 2;
// The curve up to now: thin and dashed (variant F of the probe).
constexpr double kPastWidth = 1.2;
constexpr double kDashOn = 3;
constexpr double kDashOff = 2;
constexpr int kNowRing = 7;  // white
constexpr int kNowDot = 5;   // black
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

// The points of the line from distance `from` to `to` along it.
std::vector<ColumnChart::Point> piece(const std::vector<ColumnChart::Point>& line, double from,
                                      double to) {
  std::vector<ColumnChart::Point> out;
  double at = 0;
  for (size_t i = 0; i + 1 < line.size(); i++) {
    const ColumnChart::Point& a = line[i];
    const ColumnChart::Point& b = line[i + 1];
    const double len = std::hypot(b.x - a.x, b.y - a.y);
    const double s0 = std::max(from, at), s1 = std::min(to, at + len);
    if (len > 0 && s0 <= s1) {
      auto lerp = [&](double s) {
        const double t = (s - at) / len;
        return ColumnChart::Point{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
      };
      if (out.empty()) out.push_back(lerp(s0));
      out.push_back(lerp(s1));
    }
    at += len;
    if (at >= to) break;
  }
  return out;
}

void drawDashed(ui::Canvas& c, const std::vector<ColumnChart::Point>& line) {
  double length = 0;
  for (size_t i = 0; i + 1 < line.size(); i++) {
    length += std::hypot(line[i + 1].x - line[i].x, line[i + 1].y - line[i].y);
  }
  for (double s = 0; s < length; s += kDashOn + kDashOff) {
    const std::vector<ColumnChart::Point> dash = piece(line, s, std::min(s + kDashOn, length));
    if (dash.size() >= 2) c.drawPolyline(dash, kPastWidth, Color::kBlack);
  }
}

void drawChart(ui::Canvas& c, const ColumnChart& chart) {
  if (chart.now_x < 0) {
    c.drawPolyline(chart.curve, kCurveWidth, Color::kBlack);
  } else {
    // Up to now dashed and thin, from now on solid; both meet at now.
    const ColumnChart::Point now{chart.now_x, chart.now_y};
    std::vector<ColumnChart::Point> past, future{now};
    for (const ColumnChart::Point& p : chart.curve) {
      (p.x < chart.now_x ? past : future).push_back(p);
    }
    past.push_back(now);
    drawDashed(c, past);
    if (future.size() >= 2) c.drawPolyline(future, kCurveWidth, Color::kBlack);
  }
  for (const ColumnChart::Label& l : chart.labels) {
    char t[12];
    std::snprintf(t, sizeof(t), "%d°", l.temperature);
    ui::drawText(c, ui::fonts::kRobotoCondensedBold16, l.x, l.y - 8, t, Color::kBlack,
                 ui::Align::kCenter);
    ui::drawIcon(c, l.x - kChartIconSize / 2, ColumnChart::iconTop(l),
                 ui::weatherIcon(l.icon, kChartIconSize));
  }
  if (chart.now_x >= 0) {
    const int x = roundToInt(chart.now_x), y = roundToInt(chart.now_y);
    c.fillCircle(x, y, kNowRing, Color::kWhite);
    c.fillCircle(x, y, kNowDot, Color::kBlack);
  }
}

// The forecast days after the local day of now_s, at most count.
std::vector<const Day*> laterDays(const Forecast& f, int64_t now_s, size_t count) {
  std::vector<const Day*> days;
  if (f.days.empty()) return days;
  core::LocalTime today;
  core::toLocalTime(core::isValidUnixTime(now_s) ? now_s : f.days.front().time, today);
  for (const Day& d : f.days) {
    // Daily values start at midnight at the location; noon is safely inside.
    core::LocalTime t;
    core::toLocalTime(d.time + 12 * 3600, t);
    const bool later = t.year > today.year || (t.year == today.year && t.month > today.month) ||
                       (t.year == today.year && t.month == today.month && t.day > today.day);
    if (later && days.size() < count) days.push_back(&d);
  }
  return days;
}

// "10°/3°": high and low in bold (the low a little smaller), a thin slash
// between them (2026-10-29: the low in Medium looked too thin).
void drawHighLow(ui::Canvas& c, const Day& day, int cx, int baseline) {
  const std::string high = degreesText(day.high);
  const std::string low = degreesText(day.low);
  const ui::Font& hf = ui::fonts::kRobotoCondensedBold17;
  const ui::Font& sf = ui::fonts::kRobotoCondensedMedium16;
  const ui::Font& lf = ui::fonts::kRobotoCondensedBold16;
  const int x =
      cx - (ui::textWidth(hf, high) + ui::textWidth(sf, "/") + ui::textWidth(lf, low)) / 2;
  int end = ui::drawText(c, hf, x, baseline, high);
  end = ui::drawText(c, sf, end, baseline, "/");
  ui::drawText(c, lf, end, baseline, low);
}

// A whole day: weekday, 44 px symbol of the daily code, high and low.
void drawWholeDay(ui::Canvas& c, const Day& day, int cx, ui::Language lang) {
  core::LocalTime t;
  core::toLocalTime(day.time + 12 * 3600, t);
  ui::drawText(c, ui::fonts::kRobotoCondensedBold16, cx, kDaysTop + kDays.weekday_baseline,
               ui::weekdayShort(lang, t.weekday), Color::kBlack, ui::Align::kCenter);
  ui::drawIcon(c, cx - kDays.icon_size / 2, kDaysTop + kDays.icon_top,
               ui::weatherIcon(dayIcon(day.code, day.condition), kDays.icon_size));
  drawHighLow(c, day, cx, kDaysTop + kDays.temperatures_baseline);
}

void drawDays(ui::Canvas& c, const Forecast& f, int64_t now_s, ui::Language lang, int count) {
  const int width = c.width();
  c.dottedHLine(0, kDaysTop, width, 2);
  const std::vector<const Day*> days = laterDays(f, now_s, static_cast<size_t>(count));
  const double cw = width / static_cast<double>(count);
  for (size_t i = 0; i < days.size(); i++) {
    const int cx = roundToInt(cw * i + cw / 2);
    if (i > 0)
      c.dottedVLine(roundToInt(cw * i), kDaysTop + 10, kDays.temperatures_baseline - 10, 2);
    drawWholeDay(c, *days[i], cx, lang);
  }
}

// Hours of a part, "6–10".
std::string partHours(const DayPartHours& h) {
  char buffer[16];
  std::snprintf(buffer, sizeof(buffer), "%d–%d", h.first, h.last);
  return buffer;
}

void drawPart(ui::Canvas& c, const DayPart& p, int cx) {
  const int baseline = kDaysTop + kDays.temperatures_baseline;
  if (!p.known) {
    ui::drawText(c, ui::fonts::kRobotoCondensedBold17, cx, baseline, "–°", Color::kBlack,
                 ui::Align::kCenter);
    return;
  }
  ui::drawIcon(c, cx - kDayPartIconSize / 2, kDaysTop + 50,
               ui::weatherIcon(p.icon, kDayPartIconSize));
  char t[12];
  std::snprintf(t, sizeof(t), "%d°", p.temperature);
  ui::drawText(c, ui::fonts::kRobotoCondensedBold17, cx, baseline, t, Color::kBlack,
               ui::Align::kCenter);
}

// Step L (V1): tomorrow in three parts under its weekday, then two whole days.
void drawDayParts(ui::Canvas& c, const Forecast& f, int64_t now_s, ui::Language lang) {
  const int width = c.width();
  c.dottedHLine(0, kDaysTop, width, 2);
  const std::vector<const Day*> days = laterDays(f, now_s, 3);
  if (days.empty()) return;
  const int group = 3 * kDayPartWidth;
  core::LocalTime t;
  core::toLocalTime(days[0]->time + 12 * 3600, t);
  ui::drawText(c, ui::fonts::kRobotoCondensedBold16, group / 2, kDaysTop + kDays.weekday_baseline,
               ui::weekdayShort(lang, t.weekday), Color::kBlack, ui::Align::kCenter);
  const std::vector<DayPart> parts = dayParts(f, days[0]->time + 12 * 3600);
  for (int i = 0; i < 3; i++) {
    const int cx = kDayPartWidth * i + kDayPartWidth / 2;
    if (i > 0) c.dottedVLine(kDayPartWidth * i, kDaysTop + 34, 54, 2);
    ui::drawText(c, ui::fonts::kRobotoCondensedMedium13, cx, kDaysTop + 41, partHours(kDayParts[i]),
                 Color::kBlack, ui::Align::kCenter);
    drawPart(c, parts[i], cx);
  }
  c.vLine(group, kDaysTop + 8, kDays.temperatures_baseline - 12, Color::kBlack);
  const double day_w = (width - group) / 2.0;
  for (size_t i = 1; i < days.size(); i++) {
    if (i > 1) {
      c.dottedVLine(roundToInt(group + day_w), kDaysTop + 10, kDays.temperatures_baseline - 10, 2);
    }
    drawWholeDay(c, *days[i], roundToInt(group + day_w * (i - 1) + day_w / 2), lang);
  }
}

}  // namespace

namespace {

// The hours of the chart: from `start` (local time of its first mark)
// `hours` hours on; labels at the local hours of `labels`.
struct Window {
  int64_t start = 0;
  int hours = 0;
  std::vector<int> labels;
};

// Unix time of the local hour `hour` of the day of `day_time`, from the
// hours of the forecast; 0 if that hour is not in the forecast.
int64_t hourOfDay(const Forecast& forecast, const core::LocalTime& day, int hour) {
  for (const Hour& h : forecast.hours) {
    core::LocalTime t;
    if (core::toLocalTime(h.time, t) && sameDay(t, day) && t.hour == hour) return h.time;
  }
  return 0;
}

// By night the sun of a symbol becomes the moon.
ui::WeatherIcon nightIcon(ui::WeatherIcon icon) {
  if (icon == ui::WeatherIcon::kSun) return ui::WeatherIcon::kMoon;
  if (icon == ui::WeatherIcon::kPartlyCloudy) return ui::WeatherIcon::kPartlyCloudyNight;
  return icon;
}

// 0-24 h of the local day of now_s (W3, 2026-11-24: on all steps).
Window chartWindow(const Forecast& forecast, int64_t now_s, ChartPoints points) {
  Window w;
  w.hours = 24;
  if (points == ChartPoints::kFour) {
    w.labels.assign(std::begin(kLabelHoursFour), std::end(kLabelHoursFour));
  } else {
    w.labels.assign(std::begin(kLabelHoursFive), std::end(kLabelHoursFive));
  }
  if (forecast.hours.empty()) return w;
  const int64_t now = core::isValidUnixTime(now_s) ? now_s : forecast.hours.front().time;
  core::LocalTime today;
  core::toLocalTime(now, today);
  w.start = hourOfDay(forecast, today, 0);
  if (w.start == 0) {
    // Midnight is missing: count back from the first hour of today.
    for (const Hour& h : forecast.hours) {
      core::LocalTime t;
      if (core::toLocalTime(h.time, t) && sameDay(t, today)) {
        w.start = h.time - static_cast<int64_t>(t.hour) * 3600;
        break;
      }
    }
  }
  return w;
}

}  // namespace

ColumnChart columnChart(const Forecast& forecast, int64_t now_s, int width, ChartPoints points) {
  ColumnChart chart;
  chart.curve_top = kCurveTop;
  chart.curve_bottom = kCurveBottom;
  const Window window = chartWindow(forecast, now_s, points);
  // Hours a whole number of px apart, the chart centred in the column.
  const int hours = window.hours;
  const int pitch = std::max(3, (width - 2 * kChartMargin) / hours);
  const int gx0 = (width - hours * pitch) / 2;
  auto px = [&](double h) { return gx0 + h * pitch; };
  chart.pitch = pitch;
  if (forecast.hours.empty() || window.start == 0) return chart;

  struct Item {
    int offset;  // hours after the start of the chart
    int hour;    // local hour
    const Hour* data;
  };
  std::vector<Item> items;
  for (const Hour& h : forecast.hours) {
    if (h.time < window.start || h.time > window.start + static_cast<int64_t>(hours) * 3600) {
      continue;
    }
    core::LocalTime t;
    if (!core::toLocalTime(h.time, t)) continue;
    items.push_back({static_cast<int>((h.time - window.start) / 3600), t.hour, &h});
  }
  if (items.size() < 2) return chart;

  // The temperatures of the curve: the hourly values, each averaged with its
  // neighbours (1-2-1), so that the curve runs smoothly (2026-10-29; the
  // labels keep the values of their hours).
  struct Value {
    double offset;
    float temperature;
  };
  std::vector<Value> values;
  for (size_t i = 0; i < items.size(); i++) {
    const float before = items[i == 0 ? 0 : i - 1].data->temperature;
    const float after = items[std::min(i + 1, items.size() - 1)].data->temperature;
    values.push_back({static_cast<double>(items[i].offset),
                      (before + 2 * items[i].data->temperature + after) / 4});
  }
  float tmin = values[0].temperature, tmax = tmin;
  for (const Value& v : values) {
    tmin = std::min(tmin, v.temperature);
    tmax = std::max(tmax, v.temperature);
  }
  const double span_degrees = std::max<double>(tmax - tmin, kColumnMinSpan);
  const double low = (tmin + tmax) / 2.0 - span_degrees / 2.0;
  auto py = [&](double t) {
    return chart.curve_bottom - (t - low) / span_degrees * (chart.curve_bottom - chart.curve_top);
  };

  std::vector<ColumnChart::Point> raw;
  for (const Value& v : values) raw.push_back({px(v.offset), py(v.temperature)});
  chart.curve = smooth(raw, kStepsBetweenHours);
  // The spline may swing a little beyond the points: keep it inside.
  for (ColumnChart::Point& p : chart.curve) {
    p.y = std::clamp<double>(p.y, chart.curve_top, chart.curve_bottom);
  }
  // Labels: the value of the hour, placed on the (averaged) curve, with the
  // symbol of the hours around it.
  const int after = points == ChartPoints::kFour ? 2 : 1;
  for (size_t i = 0; i < items.size(); i++) {
    const int hour = items[i].hour;
    if (std::find(window.labels.begin(), window.labels.end(), hour) == window.labels.end()) {
      continue;
    }
    ColumnChart::Label l;
    l.hour = hour;
    l.point_x = roundToInt(px(items[i].offset));
    l.x = std::clamp(l.point_x, kLabelEdge, width - kLabelEdge);
    l.y = roundToInt(py(values[i].temperature));
    l.temperature = displayTemperature(items[i].data->temperature);
    const std::vector<const Hour*> around =
        hoursOfDay(forecast, items[i].data->time, std::max(0, hour - 2), hour + after);
    l.icon = spanIcon(around);
    if (!items[i].data->day) l.icon = nightIcon(l.icon);
    chart.labels.push_back(l);
  }
  // Now on the curve, if it lies within it.
  if (core::isValidUnixTime(now_s)) {
    const double x = px((now_s - window.start) / 3600.0);
    if (x >= chart.curve.front().x && x <= chart.curve.back().x) {
      for (size_t i = 0; i + 1 < chart.curve.size(); i++) {
        const ColumnChart::Point& a = chart.curve[i];
        const ColumnChart::Point& b = chart.curve[i + 1];
        if (x < a.x || x > b.x) continue;
        const double t = b.x > a.x ? (x - a.x) / (b.x - a.x) : 0;
        chart.now_x = x;
        chart.now_y = a.y + (b.y - a.y) * t;
        break;
      }
    }
  }
  return chart;
}

void drawWeatherColumn(ui::Canvas& canvas, const Forecast& forecast, const ColumnOptions& options) {
  const bool place = !options.place.empty();
  ui::drawColumnTitle(canvas, 0, canvas.width(), ui::kColumnTitleBaseline,
                      place ? options.place : ui::text(options.language, ui::Text::kTitleWeather),
                      options.note.empty() ? kAttribution : options.note,
                      place ? ui::TitleIcon::kPin : ui::TitleIcon::kNone, options.note_alert);
  drawChart(canvas, columnChart(forecast, options.now_s, canvas.width(), options.points));
  if (options.day_parts) {
    drawDayParts(canvas, forecast, options.now_s, options.language);
  } else {
    drawDays(canvas, forecast, options.now_s, options.language, kColumnDays);
  }
}

std::vector<DayPart> dayParts(const Forecast& forecast, int64_t day_time) {
  std::vector<DayPart> parts;
  for (const DayPartHours& h : kDayParts) {
    DayPart p;
    const std::vector<const Hour*> hours = hoursOfDay(forecast, day_time, h.first, h.last);
    if (static_cast<int>(hours.size()) >= kDayPartMinHours) {
      p.known = true;
      p.condition = spanCondition(hours);
      p.icon = spanIcon(hours);
      double sum = 0;
      bool middle = false;
      for (const Hour* hour : hours) {
        sum += hour->temperature;
        core::LocalTime t;
        core::toLocalTime(hour->time, t);
        if (t.hour == h.middle) {
          p.temperature = displayTemperature(hour->temperature);
          middle = true;
        }
      }
      if (!middle) p.temperature = displayTemperature(static_cast<float>(sum / hours.size()));
    }
    parts.push_back(p);
  }
  return parts;
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
  // Step L (2026-11-16): five points; tomorrow in parts and two days (V1).
  if (step_ == ui::SizeStep::kL) {
    options.points = ChartPoints::kFive;
    options.day_parts = true;
  }
  drawWeatherColumn(canvas, source_.forecast(), options);
}

}  // namespace widgets::weather
