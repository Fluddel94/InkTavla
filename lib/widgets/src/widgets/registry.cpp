#include "widgets/registry.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "widgets/calendar/column_view.h"
#include "widgets/calendar/config.h"
#include "widgets/mvg/column_view.h"
#include "widgets/weather/column_view.h"

#if defined(ARDUINO)
#include "core/http.h"
#include "widgets/calendar/api.h"
#include "widgets/mvg/api.h"
#include "widgets/mvg/setup.h"
#include "widgets/weather/api.h"
#include "widgets/weather/setup.h"
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
       {ui::SlotSize::kBriefingWide},
       createMvg,
       [](const DataSource& s, ui::SlotSize) -> std::unique_ptr<View> {
         const auto& source = static_cast<const mvg::MvgSource&>(s);
         return std::make_unique<mvg::DeparturesColumnView>(source);
       }},
      {"weather",
       {ui::SlotSize::kBriefingColumn},
       createWeather,
       [](const DataSource& s, ui::SlotSize) -> std::unique_ptr<View> {
         const auto& source = static_cast<const weather::WeatherSource&>(s);
         return std::make_unique<weather::ForecastColumnView>(source);
       }},
      {"calendar",
       {ui::SlotSize::kBriefingColumn},
       createCalendar,
       [](const DataSource& s, ui::SlotSize) -> std::unique_ptr<View> {
         const auto& source = static_cast<const calendar::CalendarSource&>(s);
         return std::make_unique<calendar::AgendaColumnView>(source);
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

namespace {

const char* sizeName(ui::SlotSize size) {
  return size == ui::SlotSize::kBriefingWide ? "wide" : "column";
}

}  // namespace

std::string settingsLayoutJson() {
  JsonDocument doc;
  JsonArray widgets = doc["widgets"].to<JsonArray>();
  for (const WidgetType& type : widgetTypes()) {
    JsonObject w = widgets.add<JsonObject>();
    w["id"] = type.id;
    JsonArray sizes = w["sizes"].to<JsonArray>();
    for (ui::SlotSize size : type.sizes) sizes.add(sizeName(size));
  }
  JsonArray slots = doc["slots"].to<JsonArray>();
  for (ui::SlotSize size : ui::screenLayout().sizes) slots.add(sizeName(size));
  std::string text;
  serializeJson(doc, text);
  // Members only, without the braces.
  return text.substr(1, text.size() - 2);
}

#if defined(ARDUINO)
namespace {

std::string argument(const std::map<std::string, std::string>& args, const char* name) {
  const auto it = args.find(name);
  return it == args.end() ? "" : it->second;
}

}  // namespace

std::vector<core::SettingsRoute> settingsRoutes() {
  std::vector<core::SettingsRoute> routes;
  routes.push_back(
      {"mvg/stations", [](const auto& args, std::string& json) {
         const std::string query = argument(args, "q");
         if (query.size() < 2 || query.size() > 64) {
           json = "[]";
           return true;
         }
         JsonDocument doc;
         if (!core::fetchJsonStream(mvg::locationsUrl(query), doc, mvg::locationsFilter())) {
           return false;
         }
         json = mvg::stationsJson(doc.as<JsonArrayConst>());
         return true;
       }});
  routes.push_back({"mvg/lines", [](const auto& args, std::string& json) {
                      const std::string station = argument(args, "station");
                      if (!mvg::isValidStationId(station)) return false;
                      mvg::LineCatalog catalog;
                      JsonDocument doc;
                      if (!core::fetchJsonStream(mvg::linesUrl(station), doc, mvg::linesFilter())) {
                        return false;
                      }
                      catalog.addLines(doc.as<JsonArrayConst>());
                      // Example destinations; a failed request only leaves
                      // some out.
                      for (int offset : mvg::kSampleOffsets) {
                        doc.clear();
                        if (core::fetchJsonStream(mvg::sampleDeparturesUrl(station, offset), doc,
                                                  mvg::sampleDeparturesFilter())) {
                          catalog.addDepartures(doc.as<JsonArrayConst>());
                        }
                      }
                      json = catalog.toJson();
                      return true;
                    }});
  // Station by its global ID (settings page, "Advanced"): its name, which
  // also confirms that the MVG knows it.
  routes.push_back({"mvg/station", [](const auto& args, std::string& json) {
                      const std::string station = argument(args, "id");
                      if (!mvg::isValidStationId(station)) return false;
                      JsonDocument filter;
                      filter["name"] = true;
                      JsonDocument doc;
                      if (!core::fetchJsonStream(mvg::stationNameUrl(station), doc, filter)) {
                        return false;
                      }
                      const char* name = doc["name"];
                      if (name == nullptr || *name == '\0') return false;
                      JsonDocument out;
                      out["id"] = station;
                      out["name"] = name;
                      json.clear();
                      serializeJson(out, json);
                      return true;
                    }});
  routes.push_back(
      {"weather/places", [](const auto& args, std::string& json) {
         const std::string query = argument(args, "q");
         if (query.size() < 2 || query.size() > 64) {
           json = "[]";
           return true;
         }
         JsonDocument doc;
         if (!core::fetchJsonStream(weather::placeSearchUrl(query, argument(args, "lang")), doc,
                                    weather::placeSearchFilter())) {
           return false;
         }
         json = weather::placesJson(doc.as<JsonObjectConst>());
         return true;
       }});
  return routes;
}

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
