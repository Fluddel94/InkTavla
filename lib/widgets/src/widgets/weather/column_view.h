#pragma once

// View of the weather widget in a column of the "Briefing" layout
// (2026-10-21): the course of today (temperature curve from the hourly
// values, averaged with their neighbours, with five labelled points; the
// precipitation of every hour as a screened bar between its hour marks), the
// next three days with the symbol of the daily code of Open-Meteo (2026-10-28).
// On step L (2026-11-16) the chart shows the whole day 0-24 h; below it
// tomorrow in three parts (morning, midday, evening) and the two days after
// (V1, 2026-11-16).
// Title (2026-10-26): the place with the location pin, the credit
// "Open-Meteo" on the right (the age badge takes its place while the data is
// old).
//
// The bars show the amount, not the probability (2026-10-25): a bar of the
// value of 17:00 stands between the marks 16 and 17, because Open-Meteo gives
// the sum of the preceding hour. Its height follows a square root scale, so
// light rain is still visible and heavy rain does not dominate: full height
// (kColumnRainMax) from kRainFullMm, nothing below kRainMinMm. The scale
// follows the rain of the day within limits (2026-10-29, see rainBarHeight()).
//
// The chart adapts to the values so that curve and bars never meet: the
// bars (at most kColumnRainMax px) have their own strip at the bottom, the
// curve always stays above the highest bar of the day; temperatures are
// spread over at least kColumnMinSpan degrees, so a flat day stays flat
// (2026-11-24: 15 instead of 8 degrees, small differences look small: a day
// from -3 to +3 degrees fills about 40 % of the height, 15 degrees fill it).

#include <string>
#include <vector>

#include "ui/canvas.h"
#include "ui/i18n.h"
#include "ui/layout.h"
#include "widgets/weather/forecast.h"
#include "widgets/weather/widget.h"

namespace widgets::weather {

// Credit on the right of the title: the terms of Open-Meteo (CC BY 4.0) ask
// for it wherever their data is shown; the full credit with links is in the
// documentation and on the settings page.
inline constexpr char kAttribution[] = "Open-Meteo";

// Hours on the time axis of the chart (local time of the current day).
inline constexpr int kFirstHour = 5;
inline constexpr int kLastHour = 23;

inline constexpr int kColumnRainMax = 32;
inline constexpr float kRainFullMm = 5.0f;          // mm per hour for a full bar
inline constexpr float kRainFullMaxMm = 10.0f;      // full bar on the wettest days
inline constexpr float kRainMinMm = 0.1f;           // less counts as dry
inline constexpr int kRainMinBar = 2;               // lowest bar in px
inline constexpr double kRainStretchMax = 1.5;      // light rain: at most 1.5 times higher
inline constexpr double kRainStretchTop = 2.0 / 3;  // ... and at most 2/3 of the height

// Height of the bar for mm of precipitation in an hour of a day whose wettest
// hour has day_max mm. Square root scale: sqrt(min(mm / full, 1)) * stretch *
// kColumnRainMax, at least kRainMinBar, 0 below kRainMinMm.
//  - day_max up to kRainFullMm: full = kRainFullMm; days with little rain are
//    stretched (at most kRainStretchMax times) until their highest bar reaches
//    kRainStretchTop of the height; days that reach it anyway are not stretched.
//  - day_max above kRainFullMm: the wettest hour is full (full = day_max, at
//    most kRainFullMaxMm), so the hours with little rain are pressed down.
// Examples: day_max 1.1 mm: 0.1 -> 6 px, 1 -> 20; day_max 5 mm: 0.1 -> 5,
// 2 -> 20, 5 -> 32; day_max 9 mm: 0.1 -> 3, 2 -> 15, 9 -> 32.
int rainBarHeight(float mm, float day_max);
inline constexpr double kColumnMinSpan = 15;
inline constexpr int kColumnDayIconSize = 44;

// Days below the chart: three (steps S and M).
inline constexpr int kColumnDays = 3;

// Hours of the chart: 5-23 h of today (steps S and M); on step L the whole
// day 0-24 h with labels at 4, 8, 12, 16 and 20 h (2026-11-16), 24 bars.
enum class ChartSpan { kDay, kFullDay };

// Geometry of the chart (column coordinates), for drawing and tests.
struct ColumnChart {
  using Point = ui::PointF;
  struct Bar {
    int x0 = 0, x1 = 0, height = 0;  // screened from base - height to base
  };
  struct Label {
    int hour = 0;
    int x = 0, y = 0;  // point on the curve
    int temperature = 0;
  };
  int base = 0;              // bottom line of the bars
  int line_x0 = 0;           // the bottom line runs from the left edge of the
  int line_x1 = 0;           // first bar to the right edge of the last one
  int pitch = 0;             // px between two hour marks
  int curve_top = 0;         // highest possible point of the curve
  int curve_bottom = 0;      // lowest possible point of the curve
  std::vector<Point> curve;  // smoothed temperature line (not rounded to pixels)
  std::vector<Bar> bars;
  std::vector<Label> labels;  // 8, 11, 14, 17, 20 h (L: 4, 8, ..., 20 h)
};

// Chart of the local day of now_s (5-23 h, or 0-24 h with ChartSpan::kFullDay)
// for a column of this width;
// empty curve if the forecast has fewer than two hours of that day. The hour
// marks are a whole number of px apart (12 at 232 px, 11 at 214 px) and the
// chart is centred in the column, so all bars have the same width (pitch - 2)
// and the same gaps (2 px); 18 bars for 6-23 h (the value of 5 h, rain from 4
// to 5 h, is not shown).
ColumnChart columnChart(const Forecast& forecast, int64_t now_s, int width,
                        ChartSpan span = ChartSpan::kDay);

// Parts of tomorrow on step L (2026-11-16, V1, variant C of the probe):
// morning, midday and evening from the hourly values (spanCondition(), the
// symbol always by day as for the days; since W1 with intensity, hail and
// sleet by spanIcon()); the temperature of the middle hour,
// or the mean of the part if that hour is missing. Drawn under the weekday of
// tomorrow, each part with its hours ("6–10") and a 30 px symbol; a solid line
// separates them from the two whole days after, which are wider.
struct DayPartHours {
  int first, last, middle;
};
inline constexpr DayPartHours kDayParts[3] = {{6, 10, 8}, {11, 15, 13}, {16, 20, 18}};
inline constexpr int kDayPartMinHours = 2;  // fewer: no symbol, "–°"
inline constexpr int kDayPartWidth = 62;
inline constexpr int kDayPartIconSize = 30;
struct DayPart {
  bool known = false;
  Condition condition = Condition::kUnknown;
  ui::WeatherIcon icon = ui::WeatherIcon::kCloudy;
  int temperature = 0;
};
// The three parts of the local day of day_time.
std::vector<DayPart> dayParts(const Forecast& forecast, int64_t day_time);

struct ColumnOptions {
  std::string place;  // title with the pin; "" = "Wetter" without the pin
  std::string note;   // age badge instead of the credit, "" = the credit
  bool note_alert = false;
  ui::Language language = ui::Language::kGerman;
  int64_t now_s = 0;
  ChartSpan span = ChartSpan::kDay;
  bool day_parts = false;  // step L: tomorrow in three parts, then two days
};

void drawWeatherColumn(ui::Canvas& canvas, const Forecast& forecast, const ColumnOptions& options);

class ForecastColumnView : public View {
 public:
  // source must outlive the view.
  ForecastColumnView(const WeatherSource& source, ui::SizeStep step = ui::SizeStep::kS)
      : source_(source), step_(step) {}

  std::vector<const DataSource*> sources() const override { return {&source_}; }
  // The place, or "Wetter" if none is set.
  std::string title(ui::Language language) const override;
  void draw(ui::Canvas& canvas, const DrawContext& context) override;
  // The place with the pin (messages without data, no credit).
  void drawTitle(ui::Canvas& canvas, ui::Language language) override;

 private:
  const WeatherSource& source_;
  ui::SizeStep step_;
};

}  // namespace widgets::weather
