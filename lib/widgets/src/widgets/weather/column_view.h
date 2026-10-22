#pragma once

// View of the weather widget in a column of the "Briefing" layout
// (2026-10-21): the course of today (temperature curve with five labelled
// points, hourly rain probability as screened bars), the next three days
// and the credit "Open-Meteo".
//
// The chart adapts to the values so that curve and bars never meet: the
// bars (at most kColumnRainMax px) have their own strip at the bottom, the
// curve always stays above the highest bar of the day; temperatures are
// spread over at least kColumnMinSpan degrees, so a flat day stays flat.

#include <string>
#include <vector>

#include "ui/canvas.h"
#include "ui/i18n.h"
#include "widgets/column_view.h"
#include "widgets/weather/forecast.h"
#include "widgets/weather/widget.h"

namespace widgets::weather {

inline constexpr int kColumnRainMax = 30;
inline constexpr double kColumnMinSpan = 8;
inline constexpr int kColumnDayIconSize = 38;

// Geometry of the chart (column coordinates), for drawing and tests.
struct ColumnChart {
  struct Point {
    double x = 0, y = 0;
  };
  struct Bar {
    int x0 = 0, x1 = 0, height = 0;  // screened from base - height to base
  };
  struct Label {
    int hour = 0;
    int x = 0, y = 0;  // point on the curve
    int temperature = 0;
  };
  int base = 0;              // bottom line of the bars
  int curve_top = 0;         // highest possible point of the curve
  int curve_bottom = 0;      // lowest possible point of the curve
  std::vector<Point> curve;  // smoothed temperature line
  std::vector<Bar> bars;
  std::vector<Label> labels;  // 8, 11, 14, 17, 20 h
};

// Chart of the local day of now_s (5-23 h) for a column of this width;
// empty curve if the forecast has fewer than two hours of that day.
ColumnChart columnChart(const Forecast& forecast, int64_t now_s, int width);

struct ColumnOptions {
  std::string note;  // place, or the age badge
  bool note_alert = false;
  ui::Language language = ui::Language::kGerman;
  int64_t now_s = 0;
};

void drawWeatherColumn(ui::Canvas& canvas, const Forecast& forecast, const ColumnOptions& options);

class ForecastColumnView : public ColumnView {
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
