#include "widgets/weather/view.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "core/time.h"
#include "ui/elements.h"
#include "ui/fonts.h"
#include "ui/icons.h"
#include "ui/text.h"

namespace widgets::weather {

namespace {

using ui::Color;

// Positions from the overall UI design (2026-10-20), relative to the tile.
constexpr int kX0 = ui::kTilePadding;  // left edge of the content
constexpr int kTop = 40;               // top of the content below the title
// Current weather.
constexpr int kNowIconSize = 50;
// Chart.
constexpr int kChartLeft = kX0 + 172;
constexpr int kChartRightMargin = 20;  // from the tile's right edge
constexpr int kIconRow = kTop - 3;
constexpr int kCurveTop = kTop + 52;
constexpr int kCurveBottom = kTop + 68;
constexpr int kBase = kTop + 93;  // bottom line of the rain bars
constexpr int kRainBarMax = 20;   // stays below the temperature line
constexpr int kMarkIconSize = 26;
constexpr double kDash = 5;  // past part of the temperature line
constexpr double kGap = 4;
// Days.
constexpr int kDaysTop = kTop + 120;
constexpr int kDayIconSize = 32;

struct Pt {
  double x, y;
};

// Catmull-Rom spline through the points, `steps` points per segment.
std::vector<Pt> smooth(const std::vector<Pt>& p, int steps = 10) {
  std::vector<Pt> out;
  if (p.size() < 2) return p;
  for (size_t i = 0; i + 1 < p.size(); i++) {
    const Pt& p0 = p[i == 0 ? 0 : i - 1];
    const Pt& p1 = p[i];
    const Pt& p2 = p[i + 1];
    const Pt& p3 = p[std::min(i + 2, p.size() - 1)];
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

int roundToInt(double v) { return static_cast<int>(std::lround(v)); }

std::string degreesText(float t) {
  char buffer[12];
  std::snprintf(buffer, sizeof(buffer), "%d°", displayTemperature(t));
  return buffer;
}

void drawCurrent(ui::Canvas& c, const Forecast& f, const RainOutlook& rain, ui::Language lang) {
  const Current& now = f.current;
  ui::drawIcon(c, kX0 - 2, kTop + 2,
               ui::weatherIcon(conditionIcon(now.condition, now.day), kNowIconSize));
  ui::drawText(c, ui::fonts::kRobotoCondensedBold56, kX0 + 52, kTop + 48,
               degreesText(now.temperature));

  // "Heiter · gefühlt 5°", shortened to the space left of the chart.
  const ui::Font& med = ui::fonts::kRobotoCondensedMedium20;
  const ui::Font& small = ui::fonts::kRobotoCondensedMedium18;
  const std::string feels =
      std::string("· ") + ui::text(lang, ui::Text::kFeelsLike) + " " + degreesText(now.feels_like);
  const int space = kChartLeft - 14 - kX0;
  const int feels_width = ui::textWidth(small, feels) + 6;
  const std::string text =
      ui::fitText(med, conditionText(now.condition, now.day, lang), space - feels_width);
  const int end = ui::drawText(c, med, kX0, kTop + 80, text);
  ui::drawText(c, small, end + 6, kTop + 80, feels);

  // Rain outlook: "65 % 14–18 Uhr".
  const int y = kTop + 106;
  ui::drawIcon(c, kX0, y - 14, ui::kDropIcon);
  if (!rain.rain) {
    ui::drawText(c, med, kX0 + 18, y, ui::text(lang, ui::Text::kNoRain));
    return;
  }
  char pct[8];
  std::snprintf(pct, sizeof(pct), "%d %%", rain.probability);
  const int x = ui::drawText(c, ui::fonts::kRobotoCondensedBold20, kX0 + 18, y, pct);
  char range[24];
  std::snprintf(range, sizeof(range), "%d–%d%s", rain.from_hour, rain.to_hour,
                ui::text(lang, ui::Text::kClockSuffix));
  ui::drawText(c, med, x + 6, y, range);
}

void drawChart(ui::Canvas& c, const Forecast& f, int64_t now_s) {
  const int gx0 = kChartLeft;
  const int gx1 = c.width() - kChartRightMargin;
  auto px = [&](double h) {
    return gx0 + (h - kFirstHour) / (kLastHour - kFirstHour) * (gx1 - gx0);
  };

  // Hours of the current day (local time) on the axis; without a valid clock
  // the day of the first value.
  core::LocalTime today;
  core::toLocalTime(core::isValidUnixTime(now_s) ? now_s : f.hours.front().time, today);
  struct Point {
    int hour;
    const Hour* data;
  };
  std::vector<Point> points;
  for (const Hour& h : f.hours) {
    core::LocalTime t;
    if (!core::toLocalTime(h.time, t)) continue;
    if (t.year != today.year || t.month != today.month || t.day != today.day) continue;
    if (t.hour < kFirstHour || t.hour > kLastHour) continue;
    points.push_back({t.hour, &h});
  }
  if (points.size() < 2) return;

  double now_h = -1;  // position of "now" on the axis, -1 if not today
  if (core::isValidUnixTime(now_s)) {
    core::LocalTime n;
    core::toLocalTime(now_s, n);
    now_h = n.hour + n.minute / 60.0;
  }

  // Rain bars: dithered (checkerboard) with a solid 2 px top edge.
  const double step = px(kFirstHour + 1) - px(kFirstHour);
  for (const Point& p : points) {
    const int pct = p.data->rain_probability;
    if (pct <= 0) continue;
    const int h = std::max(2, roundToInt(pct / 100.0 * kRainBarMax));
    const int xa = static_cast<int>(px(p.hour) - step / 2 + 2);
    const int xb = static_cast<int>(px(p.hour) + step / 2 - 2);
    for (int y = kBase - h; y < kBase; y++) {
      for (int x = xa; x <= xb; x++) {
        if ((x + y) % 2 == 0) c.setPixel(x, y, Color::kBlack);
      }
    }
    c.fillRect(xa, kBase - h, xb - xa + 1, 2, Color::kBlack);
  }
  c.hLine(gx0 - 8, kBase, gx1 - gx0 + 15, Color::kBlack);

  // Temperature line: thin and dashed for the past, bold for what is ahead.
  float tmin = points[0].data->temperature, tmax = tmin;
  for (const Point& p : points) {
    tmin = std::min(tmin, p.data->temperature);
    tmax = std::max(tmax, p.data->temperature);
  }
  auto py = [&](double t) {
    if (tmax - tmin < 0.1) return (kCurveTop + kCurveBottom) / 2.0;
    return kCurveBottom - (t - tmin) / (tmax - tmin) * (kCurveBottom - kCurveTop);
  };
  std::vector<Pt> raw;
  for (const Point& p : points) raw.push_back({px(p.hour), py(p.data->temperature)});
  const std::vector<Pt> curve = smooth(raw, 20);
  const double nx = now_h >= 0 ? px(now_h) : -1e9;
  std::vector<Pt> past, ahead;
  for (const Pt& p : curve) {
    if (p.x <= nx) past.push_back(p);
    if (p.x >= nx) ahead.push_back(p);
  }
  // Dashes of kDash px with gaps of kGap px, measured along the line.
  double run = 0;
  for (size_t i = 0; i + 1 < past.size(); i++) {
    const double len = std::hypot(past[i + 1].x - past[i].x, past[i + 1].y - past[i].y);
    if (std::fmod(run, kDash + kGap) < kDash) {
      c.drawThickLine(roundToInt(past[i].x), roundToInt(past[i].y), roundToInt(past[i + 1].x),
                      roundToInt(past[i + 1].y), 2, Color::kBlack);
    }
    run += len;
  }
  for (size_t i = 0; i + 1 < ahead.size(); i++) {
    c.drawThickLine(roundToInt(ahead[i].x), roundToInt(ahead[i].y), roundToInt(ahead[i + 1].x),
                    roundToInt(ahead[i + 1].y), 3, Color::kBlack);
  }

  // Now: dot on the line and a dotted line down to the bars.
  if (now_h >= kFirstHour && now_h <= kLastHour) {
    const Pt* nearest = &curve[0];
    for (const Pt& p : curve) {
      if (std::fabs(p.x - nx) < std::fabs(nearest->x - nx)) nearest = &p;
    }
    const int ny = roundToInt(nearest->y);
    c.dottedVLine(roundToInt(nx), ny + 6, kBase - 1 - (ny + 6), 2);
    c.fillCircle(roundToInt(nx), ny, 5, Color::kBlack);
  }

  // Every three hours: symbol, temperature and hour; open circles ahead.
  for (const Point& p : points) {
    if ((p.hour - kFirstHour) % kMarkStep != 0) continue;
    const int x = roundToInt(px(p.hour));
    const int y = roundToInt(py(p.data->temperature));
    if (p.hour > now_h) {
      c.fillCircle(x, y, 4, Color::kWhite);
      c.drawCircle(x, y, 4, 2, Color::kBlack);
    }
    ui::drawText(c, ui::fonts::kRobotoCondensedBold18, x, y - 9, degreesText(p.data->temperature),
                 Color::kBlack, ui::Align::kCenter);
    ui::drawIcon(c, x - kMarkIconSize / 2, kIconRow,
                 ui::weatherIcon(conditionIcon(p.data->condition, p.data->day), kMarkIconSize));
    char hour[4];
    std::snprintf(hour, sizeof(hour), "%d", p.hour);
    ui::drawText(c, ui::fonts::kRobotoCondensedMedium17, x, kBase + 19, hour, Color::kBlack,
                 ui::Align::kCenter);
  }
}

void drawDays(ui::Canvas& c, const Forecast& f, int64_t now_s, ui::Language lang) {
  const int width = c.width();
  c.dottedHLine(0, kDaysTop, width, 2);
  // The three days after today (local date of the location).
  core::LocalTime today;
  core::toLocalTime(core::isValidUnixTime(now_s) ? now_s : f.days.front().time, today);
  std::vector<const Day*> days;
  for (const Day& d : f.days) {
    // Daily values start at midnight at the location; noon is safely inside
    // the day in any time zone the device might use.
    core::LocalTime t;
    core::toLocalTime(d.time + 12 * 3600, t);
    const bool later = t.year > today.year || (t.year == today.year && t.month > today.month) ||
                       (t.year == today.year && t.month == today.month && t.day > today.day);
    if (later && days.size() < 3) days.push_back(&d);
  }
  const double cw = width / 3.0;
  const int yb = kDaysTop + 38;
  const ui::Font& bold = ui::fonts::kRobotoCondensedBold22;
  const ui::Font& med = ui::fonts::kRobotoCondensedMedium20;
  for (size_t i = 0; i < days.size(); i++) {
    const int cx = static_cast<int>(i * cw);
    if (i > 0) c.dottedVLine(cx, kDaysTop + 8, c.height() - 8 - (kDaysTop + 8), 2);
    core::LocalTime t;
    core::toLocalTime(days[i]->time + 12 * 3600, t);
    ui::drawText(c, bold, cx + 14, yb, ui::weekdayShort(lang, t.weekday));
    ui::drawIcon(c, cx + 41, yb - 28,
                 ui::weatherIcon(conditionIcon(days[i]->condition, true), kDayIconSize));
    const int right = static_cast<int>(cx + cw) - 14;
    const std::string low = degreesText(days[i]->low);
    ui::drawText(c, med, right, yb, low, Color::kBlack, ui::Align::kRight);
    ui::drawText(c, bold, right - 6 - ui::textWidth(med, low), yb, degreesText(days[i]->high),
                 Color::kBlack, ui::Align::kRight);
  }
}

}  // namespace

void drawWeather(ui::Canvas& canvas, const Forecast& forecast, const ViewOptions& options) {
  ui::drawTileTitle(canvas, options.title + " · " + kAttribution, options.note, options.note_alert);
  if (forecast.hours.empty() || forecast.days.empty()) return;
  drawCurrent(canvas, forecast, rainOutlook(forecast, options.now_s), options.language);
  drawChart(canvas, forecast, options.now_s);
  drawDays(canvas, forecast, options.now_s, options.language);
}

}  // namespace widgets::weather
