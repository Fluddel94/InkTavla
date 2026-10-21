#include "widgets/placeholder.h"

#include "ui/elements.h"

namespace widgets {

PlaceholderView PlaceholderView::notAvailable(std::string id) {
  return PlaceholderView(std::move(id), ui::Text::kNotSetUp, "");
}

PlaceholderView PlaceholderView::doesNotFit(std::string id) {
  return PlaceholderView(std::move(id), ui::Text::kDoesNotFit, "config.json: layout.slots");
}

std::string PlaceholderView::title(ui::Language language) const {
  if (id_ == "weather") return ui::text(language, ui::Text::kTitleWeather);
  if (id_ == "calendar") return ui::text(language, ui::Text::kTitleCalendar);
  if (id_ == "mvg") return ui::text(language, ui::Text::kTitleDepartures);
  return id_;
}

void PlaceholderView::draw(ui::Canvas& canvas, const DrawContext& context) {
  ui::drawTileTitle(canvas, title(context.language));
  ui::drawTileMessage(
      canvas, ui::text(context.language, message_),
      detail_.empty() ? ui::text(context.language, ui::Text::kNotAvailable) : detail_);
}

}  // namespace widgets
