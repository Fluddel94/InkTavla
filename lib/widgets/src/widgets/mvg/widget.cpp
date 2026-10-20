#include "widgets/mvg/widget.h"

#include <utility>

#include "widgets/mvg/sections.h"

namespace widgets::mvg {

MvgWidget::MvgWidget(Config config, Source& source) : config_(std::move(config)), source_(source) {}

std::string MvgWidget::title(ui::Language language) const {
  return config_.name.empty() ? ui::text(language, ui::Text::kTitleDepartures) : config_.name;
}

SetupMessage MvgWidget::setupMessage(ui::Language language) const {
  return {ui::text(language, ui::Text::kStationMissing), "config.json: widgets.mvg.station"};
}

bool MvgWidget::fetch(int64_t wall_s) {
  (void)wall_s;
  JsonDocument doc;
  if (!source_.fetchDepartures(config_, doc)) return false;
  if (!doc.is<JsonArrayConst>()) return false;  // not a departures response
  sections_ = departureSections(doc, config_, kMaxDepartures);
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
