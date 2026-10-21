#include "widgets/weather/widget.h"

#include <utility>

#include "core/psram.h"
#include "widgets/weather/view.h"

namespace widgets::weather {

WeatherSource::WeatherSource(Config config, Source& api) : config_(std::move(config)), api_(api) {}

SetupMessage WeatherSource::setupMessage(ui::Language language) const {
  return {ui::text(language, ui::Text::kLocationMissing),
          "config.json: widgets.weather.latitude/longitude"};
}

bool WeatherSource::fetch(int64_t wall_s) {
  (void)wall_s;
  JsonDocument doc(core::jsonAllocator());
  if (!api_.fetchForecast(config_, doc)) return false;
  Forecast forecast;
  if (!parseForecast(doc, forecast)) return false;
  forecast_ = std::move(forecast);
  return true;
}

std::string ForecastView::title(ui::Language language) const {
  return ui::text(language, ui::Text::kTitleWeather);
}

void ForecastView::draw(ui::Canvas& canvas, const DrawContext& context) {
  ViewOptions options;
  options.title = title(context.language);
  // The age badge replaces the place while the data is old.
  options.note = context.note.empty() ? source_.config().name : context.note;
  options.note_alert = context.note_alert;
  options.language = context.language;
  options.now_s = context.now_s;
  drawWeather(canvas, source_.forecast(), options);
}

}  // namespace widgets::weather
