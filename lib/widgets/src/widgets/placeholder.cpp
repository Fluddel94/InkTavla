#include "widgets/placeholder.h"

namespace widgets {

std::string PlaceholderWidget::title(ui::Language language) const {
  if (id_ == "weather") return ui::text(language, ui::Text::kTitleWeather);
  if (id_ == "calendar") return ui::text(language, ui::Text::kTitleCalendar);
  if (id_ == "mvg") return ui::text(language, ui::Text::kTitleDepartures);
  return id_;
}

SetupMessage PlaceholderWidget::setupMessage(ui::Language language) const {
  return {ui::text(language, ui::Text::kNotSetUp), ui::text(language, ui::Text::kNotAvailable)};
}

}  // namespace widgets
