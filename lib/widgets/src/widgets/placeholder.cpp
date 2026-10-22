#include "widgets/placeholder.h"

#include "ui/elements.h"

namespace widgets {

PlaceholderView PlaceholderView::notAvailable(std::string id, bool column_style) {
  return PlaceholderView(std::move(id), ui::Text::kNotSetUp, "", column_style);
}

PlaceholderView PlaceholderView::doesNotFit(std::string id, bool column_style) {
  return PlaceholderView(std::move(id), ui::Text::kDoesNotFit, "config.json: layout.slots",
                         column_style);
}

std::string PlaceholderView::title(ui::Language language) const {
  if (id_ == "weather") return ui::text(language, ui::Text::kTitleWeather);
  if (id_ == "calendar") return ui::text(language, ui::Text::kTitleCalendar);
  if (id_ == "mvg") return ui::text(language, ui::Text::kTitleDepartures);
  return id_;
}

void PlaceholderView::draw(ui::Canvas& canvas, const DrawContext& context) {
  const std::string detail =
      detail_.empty() ? ui::text(context.language, ui::Text::kNotAvailable) : detail_;
  if (column_style_) {
    ui::drawColumnTitle(canvas, 0, canvas.width(), ui::kColumnTitleBaseline,
                        title(context.language));
    ui::drawColumnMessage(canvas, ui::text(context.language, message_), detail);
    return;
  }
  ui::drawTileTitle(canvas, title(context.language));
  ui::drawTileMessage(canvas, ui::text(context.language, message_), detail);
}

}  // namespace widgets
