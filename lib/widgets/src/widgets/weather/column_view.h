#pragma once

// View of the weather widget in a column of the "Briefing" layout
// (2026-10-21): the course of today (temperature curve from the values every
// 15 minutes, or hourly if there are none, with five labelled points; the
// precipitation of every hour as a screened bar between its hour marks), the
// next three days and the credit "Open-Meteo".
//
// The bars show the amount, not the probability (2026-10-25): a bar of the
// value of 17:00 stands between the marks 16 and 17, because Open-Meteo gives
// the sum of the preceding hour. Its height follows a square root scale, so
// light rain is still visible and heavy rain does not dominate: full height
// (kColumnRainMax) from kRainFullMm, nothing below kRainMinMm.
//
// The chart adapts to the values so that curve and bars never meet: the
// bars (at most kColumnRainMax px) have their own strip at the bottom, the
// curve always stays above the highest bar of the day; temperatures are
// spread over at least kColumnMinSpan degrees, so a flat day stays flat.

#include <string>
#include <vector>

#include "ui/canvas.h"
#include "ui/i18n.h"
#include "widgets/weather/forecast.h"
#include "widgets/weather/widget.h"

namespace widgets::weather {

// Credit shown under the column: the terms of Open-Meteo ask for it
// wherever their data is shown.
inline constexpr char kAttribution[] = "Open-Meteo";

// Hours on the time axis of the chart (local time of the current day).
inline constexpr int kFirstHour = 5;
inline constexpr int kLastHour = 23;

inline constexpr int kColumnRainMax = 30;
inline constexpr float kRainFullMm = 5.0f;  // mm per hour for a full bar
inline constexpr float kRainMinMm = 0.1f;   // less counts as dry
inline constexpr int kRainMinBar = 2;       // lowest bar in px

// Height of the bar for mm of precipitation in an hour:
// max(kRainMinBar, round(sqrt(min(mm / kRainFullMm, 1)) * kColumnRainMax)),
// 0 below kRainMinMm (0.2 mm -> 6 px, 0.5 -> 9, 2 -> 19, 5 and more -> 30).
int rainBarHeight(float mm);
inline constexpr double kColumnMinSpan = 8;
inline constexpr int kColumnDayIconSize = 38;

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
  std::vector<Label> labels;  // 8, 11, 14, 17, 20 h
};

// Chart of the local day of now_s (5-23 h) for a column of this width;
// empty curve if the forecast has fewer than two hours of that day. The hour
// marks are a whole number of px apart (12 at 232 px, 11 at 214 px) and the
// chart is centred in the column, so all bars have the same width (pitch - 2)
// and the same gaps (2 px); 18 bars for 6-23 h (the value of 5 h, rain from 4
// to 5 h, is not shown).
ColumnChart columnChart(const Forecast& forecast, int64_t now_s, int width);

struct ColumnOptions {
  std::string note;  // place, or the age badge
  bool note_alert = false;
  ui::Language language = ui::Language::kGerman;
  int64_t now_s = 0;
};

void drawWeatherColumn(ui::Canvas& canvas, const Forecast& forecast, const ColumnOptions& options);

class ForecastColumnView : public View {
 public:
  // source must outlive the view.
  explicit ForecastColumnView(const WeatherSource& source) : source_(source) {}

  std::vector<const DataSource*> sources() const override { return {&source_}; }
  std::string title(ui::Language language) const override;
  void draw(ui::Canvas& canvas, const DrawContext& context) override;

 private:
  const WeatherSource& source_;
};

}  // namespace widgets::weather
