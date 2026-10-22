#include "widgets/registry.h"

#include <algorithm>
#include <utility>

#include "widgets/calendar/column_view.h"
#include "widgets/calendar/config.h"
#include "widgets/mvg/column_view.h"
#include "widgets/weather/column_view.h"

#if defined(ARDUINO)
#include "widgets/calendar/api.h"
#include "widgets/mvg/api.h"
#include "widgets/weather/api.h"
#endif

namespace widgets {

namespace {

std::unique_ptr<DataSource> createMvg(JsonObjectConst root, const Services& services,
                                      core::ConfigIssues& issues) {
  mvg::Config config = mvg::parseConfig(core::widgetSection(root, "mvg"), issues);
  if (services.mvg == nullptr) return nullptr;
  return std::make_unique<mvg::MvgSource>(std::move(config), *services.mvg);
}

std::unique_ptr<DataSource> createWeather(JsonObjectConst root, const Services& services,
                                          core::ConfigIssues& issues) {
  weather::Config config = weather::parseConfig(core::widgetSection(root, "weather"), issues);
  if (services.weather == nullptr) return nullptr;
  return std::make_unique<weather::WeatherSource>(std::move(config), *services.weather);
}

std::unique_ptr<DataSource> createCalendar(JsonObjectConst root, const Services& services,
                                           core::ConfigIssues& issues) {
  calendar::checkConfig(core::widgetSection(root, "calendar"), issues);
  if (services.calendar == nullptr) return nullptr;
  return std::make_unique<calendar::CalendarSource>(*services.calendar);
}

}  // namespace

bool WidgetType::fits(ui::SlotSize size) const {
  return std::find(sizes.begin(), sizes.end(), size) != sizes.end();
}

const std::vector<WidgetType>& widgetTypes() {
  static const std::vector<WidgetType> types = {
      {"mvg",
       {ui::SlotSize::kColumn, ui::SlotSize::kBriefingWide},
       createMvg,
       [](const DataSource& s, ui::SlotSize size) -> std::unique_ptr<View> {
         const auto& source = static_cast<const mvg::MvgSource&>(s);
         if (size == ui::SlotSize::kBriefingWide) {
           return std::make_unique<mvg::DeparturesColumnView>(source);
         }
         return std::make_unique<mvg::DeparturesView>(source);
       }},
      {"weather",
       {ui::SlotSize::kTile, ui::SlotSize::kBriefingColumn},
       createWeather,
       [](const DataSource& s, ui::SlotSize size) -> std::unique_ptr<View> {
         const auto& source = static_cast<const weather::WeatherSource&>(s);
         if (size == ui::SlotSize::kBriefingColumn) {
           return std::make_unique<weather::ForecastColumnView>(source);
         }
         return std::make_unique<weather::ForecastView>(source);
       }},
      {"calendar",
       {ui::SlotSize::kTile, ui::SlotSize::kBriefingColumn},
       createCalendar,
       [](const DataSource& s, ui::SlotSize size) -> std::unique_ptr<View> {
         const auto& source = static_cast<const calendar::CalendarSource&>(s);
         if (size == ui::SlotSize::kBriefingColumn) {
           return std::make_unique<calendar::AgendaColumnView>(source);
         }
         return std::make_unique<calendar::AgendaView>(source);
       }},
  };
  return types;
}

const WidgetType* findWidgetType(std::string_view id) {
  for (const WidgetType& type : widgetTypes()) {
    if (id == type.id) return &type;
  }
  return nullptr;
}

#if defined(ARDUINO)
Services deviceServices() {
  static mvg::ApiSource mvg_api;
  static weather::ApiSource weather_api;
  static calendar::ApiSource calendar_api;
  static bool started = false;
  if (!started) {
    calendar_api.begin();  // before the sources ask whether it is set up
    started = true;
  }
  Services s;
  s.mvg = &mvg_api;
  s.weather = &weather_api;
  s.calendar = &calendar_api;
  return s;
}
#endif

}  // namespace widgets
