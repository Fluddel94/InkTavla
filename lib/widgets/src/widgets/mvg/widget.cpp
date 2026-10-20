#include "widgets/mvg/widget.h"

#include <utility>

#include "core/psram.h"
#include "widgets/mvg/sections.h"

namespace widgets::mvg {

MvgWidget::MvgWidget(Config config, Source& source) : config_(std::move(config)), source_(source) {}

std::string MvgWidget::title(ui::Language language) const {
  if (!config_.name.empty()) return config_.name;
  if (!station_name_.empty()) return station_name_;
  return ui::text(language, ui::Text::kTitleDepartures);
}

SetupMessage MvgWidget::setupMessage(ui::Language language) const {
  return {ui::text(language, ui::Text::kStationMissing), "config.json: widgets.mvg.station"};
}

bool MvgWidget::fetch(int64_t wall_s) {
  (void)wall_s;
  JsonDocument doc(core::jsonAllocator());  // large: PSRAM on the device
  if (!source_.fetchDepartures(config_, doc)) return false;
  if (!doc.is<JsonArrayConst>()) return false;  // not a departures response
  sections_ = departureSections(doc, config_, kMaxDepartures);
  // The station name once, after the departures (a failure here is not a
  // failure of the update; it is tried again next time).
  if (config_.name.empty() && station_name_.empty()) {
    source_.fetchStationName(config_.station, station_name_);
  }
  return true;
}

void MvgWidget::draw(ui::Canvas& canvas, const DrawContext& context) {
  const std::vector<Section> visible = withoutDeparted(sections_, context.now_s);
  ViewOptions options;
  options.title = title(context.language);
  options.note = context.note;
  options.note_alert = context.note_alert;
  options.time_format = config_.time_format;
  options.language = context.language;
  options.now_s = context.now_s;
  drawDepartures(canvas, visible, options);
}

}  // namespace widgets::mvg
