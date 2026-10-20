#include "widgets/app.h"

#include "core/time.h"
#include "widgets/mvg/config.h"
#include "widgets/placeholder.h"

namespace widgets {

std::unique_ptr<Widget> createWidget(const std::string& id, JsonObjectConst root,
                                     const Services& services, core::ConfigIssues& issues) {
  if (id == "mvg" && services.mvg != nullptr) {
    return std::make_unique<mvg::MvgWidget>(
        mvg::parseConfig(core::widgetSection(root, "mvg"), issues), *services.mvg);
  }
  if (id != "mvg" && id != "weather" && id != "calendar") {
    issues.push_back({"layout.slots", "unknown widget \"" + id + "\""});
  }
  return std::make_unique<PlaceholderWidget>(id);
}

App::App(core::Clock& clock, hal::Display& display, const Services& services)
    : clock_(clock), display_(display), services_(services) {}

void App::configure(JsonObjectConst root, core::ConfigIssues& issues) {
  config_ = core::parseCoreConfig(root, issues);
  core::setTimeZone(config_.time_zone.c_str());
  const ui::Language language = ui::languageFromCode(config_.language);
  const ui::Layout layout = ui::layoutFor(config_.layout);

  slots_.clear();
  std::vector<Widget*> widgets{&header_};
  for (size_t i = 0; i < layout.slots.size(); i++) {
    const std::string id = i < config_.slots.size() ? config_.slots[i] : "";
    slots_.push_back(createWidget(id, root, services_, issues));
    widgets.push_back(slots_.back().get());
  }

  screen_ = std::make_unique<Screen>(layout, language);
  screen_->setWidgets(widgets);
  controller_ = std::make_unique<core::DisplayController>(clock_, display_, *screen_,
                                                          config_.full_refresh_minutes);
  for (Widget* w : widgets) controller_->addContent(*w);
}

}  // namespace widgets
