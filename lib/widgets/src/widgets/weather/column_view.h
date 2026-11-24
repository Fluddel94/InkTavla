#pragma once

// View of the weather widget in a column of the "Briefing" layout
// (2026-10-21): the course of today and the next three days with the symbol
// of the daily code of Open-Meteo (2026-10-28). On step L (2026-11-16)
// tomorrow follows in three parts (morning, midday, evening) and then the two
// days after (V1, 2026-11-16).
// Title (2026-10-26): the place with the location pin, the credit
// "Open-Meteo" on the right (the age badge takes its place while the data is
// old).
//
// The chart (W3, 2026-11-24): the temperature curve of the whole day 0-24 h
// from the hourly values, averaged with their neighbours (1-2-1), without
// rain bars, hour numbers or a bottom line. It reaches down to the dotted
// line of the days. Labelled points (S: 5, 10, 15, 20 h; L: 6, 10, 14, 18,
// 22 h) show the temperature of the hour and above it the 30 px symbol of
// the hours around it (by night with the moon). The curve is thin and
// dashed up to now and solid from now on; a black dot in a white ring marks
// now on the curve. Temperatures are spread over at least kColumnMinSpan
// degrees, so a flat day stays flat (2026-11-24: 15 instead of 8 degrees: a
// day from -3 to +3 degrees fills about 40 % of the height, 15 degrees fill
// it).

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

inline constexpr double kColumnMinSpan = 15;
inline constexpr int kColumnDayIconSize = 44;

// Days below the chart: three (steps S and M).
inline constexpr int kColumnDays = 3;

// Labelled points of the chart: four on steps S and M (5, 10, 15, 20 h),
// five on step L (6, 10, 14, 18, 22 h). The chart always shows 0-24 h.
enum class ChartPoints { kFour, kFive };
inline constexpr int kChartIconSize = 30;

// Geometry of the chart (column coordinates), for drawing and tests.
struct ColumnChart {
  using Point = ui::PointF;
  struct Label {
    int hour = 0;
    int point_x = 0, y = 0;  // point on the curve
    int x = 0;               // centre of temperature and symbol (kept off the edge)
    int temperature = 0;
    ui::WeatherIcon icon = ui::WeatherIcon::kCloudy;
  };
  int pitch = 0;             // px between two hours
  int curve_top = 0;         // highest possible point of the curve
  int curve_bottom = 0;      // lowest possible point of the curve
  std::vector<Point> curve;  // smoothed temperature line (not rounded to pixels)
  std::vector<Label> labels;
  // Now on the curve; now_x is -1 if now is not within the curve of today
  // (then the curve is solid and has no dot).
  double now_x = -1, now_y = 0;
  // The symbol of a label: the temperature baseline is y - 8, the digits are
  // 12 px high, the symbol ends 4 px above them.
  static int iconTop(const Label& l) { return l.y - 8 - 12 - 4 - kChartIconSize; }
};

// Chart of the local day of now_s (0-24 h) for a column of this width;
// empty curve if the forecast has fewer than two hours of that day. The
// hours are a whole number of px apart (8 at 214 px, 14 at 367 px) and the
// chart is centred in the column. The symbol of a label is spanIcon() of the
// hours around it (four points: h-2 to h+2, five: h-2 to h+1); by night
// (Hour::day false at h) the sun becomes the moon.
ColumnChart columnChart(const Forecast& forecast, int64_t now_s, int width,
                        ChartPoints points = ChartPoints::kFour);

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
  ChartPoints points = ChartPoints::kFour;
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
