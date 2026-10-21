#include "widgets/mvg/widget.h"

#include <utility>

#include "core/psram.h"
#include "widgets/mvg/sections.h"

namespace widgets::mvg {

MvgSource::MvgSource(Config config, Source& api) : config_(std::move(config)), api_(api) {}

SetupMessage MvgSource::setupMessage(ui::Language language) const {
  return {ui::text(language, ui::Text::kStationMissing), "config.json: widgets.mvg.station"};
}

const std::string& MvgSource::stationName() const {
  return config_.name.empty() ? station_name_ : config_.name;
}

bool MvgSource::fetch(int64_t wall_s) {
  (void)wall_s;
  JsonDocument doc(core::jsonAllocator());  // large: PSRAM on the device
  if (!api_.fetchDepartures(config_, doc)) return false;
  if (!doc.is<JsonArrayConst>()) return false;  // not a departures response
  sections_ = departureSections(doc, config_, kMaxDepartures);
  // The station name once, after the departures (a failure here is not a
  // failure of the update; it is tried again next time).
  if (config_.name.empty() && station_name_.empty()) {
    api_.fetchStationName(config_.station, station_name_);
  }
  return true;
}

std::string DeparturesView::title(ui::Language language) const {
  const std::string& name = source_.stationName();
  return name.empty() ? std::string(ui::text(language, ui::Text::kTitleDepartures)) : name;
}

void DeparturesView::draw(ui::Canvas& canvas, const DrawContext& context) {
  const std::vector<Section> visible = withoutDeparted(source_.sections(), context.now_s);
  ViewOptions options;
  options.title = title(context.language);
  options.note = context.note;
  options.note_alert = context.note_alert;
  options.time_format = source_.config().time_format;
  options.language = context.language;
  options.now_s = context.now_s;
  drawDepartures(canvas, visible, options);
}

}  // namespace widgets::mvg
