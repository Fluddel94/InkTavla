#include "widgets/app.h"

#include <cstring>
#include <string>

#include "core/time.h"
#include "ui/i18n.h"
#include "ui/layout.h"
#include "widgets/briefing/band.h"
#include "widgets/placeholder.h"

namespace widgets {

namespace {

// Builds the views of the layout's columns and the data sources they read.
// With empty services only the configuration is checked (no sources).
class Builder {
 public:
  Builder(JsonObjectConst root, const Services& services, core::ConfigIssues& issues)
      : root_(root), services_(services), issues_(issues) {}

  std::unique_ptr<View> viewFor(const std::string& id, size_t slot, ui::SlotSize size) {
    const WidgetType* type = findWidgetType(id);
    if (type == nullptr) {
      issues_.push_back({"layout.slots", "unknown widget \"" + id + "\""});
      return std::make_unique<PlaceholderView>(PlaceholderView::notAvailable(id));
    }
    if (!type->fits(size)) {
      std::string fits;
      for (ui::SlotSize s : type->sizes) {
        fits += std::string(fits.empty() ? "" : ", ") + ui::slotSizeName(s);
      }
      issues_.push_back({"layout.slots", "widget \"" + id + "\" does not fit slot " +
                                             std::to_string(slot + 1) + " (" +
                                             ui::slotSizeName(size) + "); it fits: " + fits});
      checkSettings(*type);  // no source (nothing to fetch), but the settings are checked
      return std::make_unique<PlaceholderView>(PlaceholderView::doesNotFit(id));
    }
    DataSource* source = sourceFor(*type);
    if (source == nullptr) {
      return std::make_unique<PlaceholderView>(PlaceholderView::notAvailable(id));
    }
    return type->create_view(*source, size);
  }

  // The data source of a widget for the band, nullptr if it cannot be
  // created.
  const DataSource* sourceById(const char* id) {
    const WidgetType* type = findWidgetType(id);
    return type == nullptr ? nullptr : sourceFor(*type);
  }

  // The data source of a widget if one of the columns uses it already.
  const DataSource* existingSource(const char* id) const {
    for (const auto& s : sources_) {
      if (std::strcmp(s->id(), id) == 0) return s.get();
    }
    return nullptr;
  }

  std::vector<std::unique_ptr<DataSource>>& sources() { return sources_; }

 private:
  // One source per widget, shared by all its views; created (and its
  // settings read) on first use.
  DataSource* sourceFor(const WidgetType& type) {
    for (const auto& s : sources_) {
      if (std::strcmp(s->id(), type.id) == 0) return s.get();
    }
    if (contains(missing_, type.id)) return nullptr;
    // Settings that were checked already are not reported twice.
    core::ConfigIssues repeated;
    const bool checked = contains(checked_, type.id);
    std::unique_ptr<DataSource> source =
        type.create_source(root_, services_, checked ? repeated : issues_);
    if (!checked) checked_.push_back(type.id);
    if (source == nullptr) {
      missing_.push_back(type.id);  // no service for it
      return nullptr;
    }
    sources_.push_back(std::move(source));
    return sources_.back().get();
  }

  // Reports problems of the widget's settings without creating its source.
  void checkSettings(const WidgetType& type) {
    if (contains(checked_, type.id)) return;
    checked_.push_back(type.id);
    type.create_source(root_, Services{}, issues_);
  }

  static bool contains(const std::vector<const char*>& ids, const char* id) {
    for (const char* i : ids) {
      if (std::strcmp(i, id) == 0) return true;
    }
    return false;
  }

  JsonObjectConst root_;
  const Services& services_;
  core::ConfigIssues& issues_;
  std::vector<std::unique_ptr<DataSource>> sources_;
  std::vector<const char*> checked_;  // widgets whose settings were read
  std::vector<const char*> missing_;  // widgets without a service
};

}  // namespace

void validateConfig(JsonObjectConst root, core::ConfigIssues& issues) {
  const core::CoreConfig config = core::parseCoreConfig(root, issues);
  const ui::Layout layout = ui::screenLayout();
  Builder builder(root, Services{}, issues);
  for (size_t i = 0; i < layout.slots.size(); i++) {
    builder.viewFor(i < config.slots.size() ? config.slots[i] : "", i, layout.sizes[i]);
  }
  builder.sourceById("weather");  // the band reads the weather
}

App::App(core::Clock& clock, hal::Display& display, const Services& services)
    : clock_(clock), display_(display), services_(services) {}

void App::configure(JsonObjectConst root, core::ConfigIssues& issues) {
  config_ = core::parseCoreConfig(root, issues);
  core::setTimeZone(config_.time_zone.c_str());
  const ui::Language language = ui::languageFromCode(config_.language);
  ui::LayoutOptions options;
  options.equal_columns = config_.equal_columns;
  options.dividers = config_.dividers;
  const ui::Layout layout = ui::screenLayout(options);

  Builder builder(root, services_, issues);
  views_.clear();
  std::vector<View*> views{nullptr};  // the band, see below
  for (size_t i = 0; i < layout.slots.size(); i++) {
    const std::string id = i < config_.slots.size() ? config_.slots[i] : "";
    views_.push_back(builder.viewFor(id, i, layout.sizes[i]));
    views.push_back(views_.back().get());
  }
  // The band reads the weather even if the weather column is not on the
  // screen; the departures only if they are (nothing is fetched just for the
  // sentence). The calendar only for the note on failed updates.
  const auto* weather = static_cast<const weather::WeatherSource*>(builder.sourceById("weather"));
  const auto* departures = static_cast<const mvg::MvgSource*>(builder.existingSource("mvg"));
  const DataSource* calendar = builder.existingSource("calendar");
  views_.push_back(std::make_unique<briefing::BandView>(weather, departures, calendar));
  views[0] = views_.back().get();
  sources_ = std::move(builder.sources());

  screen_ = std::make_unique<Screen>(layout, language);
  controller_ = std::make_unique<core::DisplayController>(clock_, display_, *screen_,
                                                          config_.full_refresh_minutes);
  controller_->enableStartScreen();
  std::vector<const DataSource*> order;
  for (const auto& s : sources_) {
    controller_->addContent(*s);
    order.push_back(s.get());
  }
  screen_->setSources(order);
  screen_->setViews(views);
  screen_->setSystemReport([this] { return systemReport(); });
}

core::SystemReport App::systemReport() const {
  core::SystemReport report;
  if (device_info_) report.device = device_info_();
  if (controller_ != nullptr) report.sources = controller_->sourceReports();
  const core::SystemLog& log = core::systemLog();
  report.errors = log.count(core::LogLevel::kError);
  report.warnings = log.count(core::LogLevel::kWarning) - report.errors;
  return report;
}

}  // namespace widgets
