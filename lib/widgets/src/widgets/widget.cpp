#include "widgets/widget.h"

#include "ui/elements.h"

namespace widgets {

SetupMessage DataSource::setupMessage(ui::Language language) const {
  return {ui::text(language, ui::Text::kNotSetUp), ""};
}

void View::drawTitle(ui::Canvas& canvas, ui::Language language) {
  ui::drawTileTitle(canvas, title(language));
}

void View::drawMessage(ui::Canvas& canvas, ui::Language language, std::string_view message,
                       std::string_view detail) {
  ui::drawTileTitle(canvas, title(language));
  ui::drawTileMessage(canvas, message, detail);
}

}  // namespace widgets
