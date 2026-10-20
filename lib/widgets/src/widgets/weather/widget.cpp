#include "widgets/weather/widget.h"

#include <utility>

#include "core/psram.h"
#include "widgets/weather/view.h"

namespace widgets::weather {

WeatherWidget::WeatherWidget(Config config, Source& source)
    : config_(std::move(config)), source_(source) {}

std::string WeatherWidget::title(ui::Language language) const {
  return ui::text(language, ui::Text::kTitleWeather);
}

SetupMessage WeatherWidget::setupMessage(ui::Language language) const {
  return {ui::text(language, ui::Text::kLocationMissing),
          "config.json: widgets.weather.latitude/longitude"};
}

bool WeatherWidget::fetch(int64_t wall_s) {
  (void)wall_s;
  JsonDocument doc(core::jsonAllocator());
  if (!source_.fetchForecast(config_, doc)) return false;
  Forecast forecast;
  if (!parseForecast(doc, forecast)) return false;
  forecast_ = std::move(forecast);
  return true;
}

void WeatherWidget::draw(ui::Canvas& canvas, const DrawContext& context) {
  ViewOptions options;
  options.title = title(context.language);
  // The age badge replaces the place while the data is old.
  options.note = context.note.empty() ? config_.name : context.note;
  options.note_alert = context.note_alert;
  options.language = context.language;
  options.now_s = context.now_s;
  drawWeather(canvas, forecast_, options);
}

}  // namespace widgets::weather
