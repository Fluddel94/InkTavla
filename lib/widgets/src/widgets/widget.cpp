#include "widgets/widget.h"

#include "ui/elements.h"

namespace widgets {

SetupMessage DataSource::setupMessage(ui::Language language) const {
  return {ui::text(language, ui::Text::kNotSetUp), ""};
}

void View::drawTitle(ui::Canvas& canvas, ui::Language language) {
  ui::drawColumnTitle(canvas, 0, canvas.width(), ui::kColumnTitleBaseline, title(language));
}

void View::drawMessage(ui::Canvas& canvas, ui::Language language, std::string_view message,
                       std::string_view detail, bool warning) {
  drawTitle(canvas, language);
  ui::drawColumnMessage(canvas, message, detail, warning);
}

}  // namespace widgets
