#include "widgets/widget.h"

namespace widgets {

SetupMessage Widget::setupMessage(ui::Language language) const {
  return {ui::text(language, ui::Text::kNotSetUp), ""};
}

}  // namespace widgets
