#include "widgets/widget.h"

namespace widgets {

SetupMessage DataSource::setupMessage(ui::Language language) const {
  return {ui::text(language, ui::Text::kNotSetUp), ""};
}

}  // namespace widgets
