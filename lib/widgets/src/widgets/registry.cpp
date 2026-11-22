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
#include <ctime>

#include "core/http.h"
#include "core/secrets.h"
#include "core/syslog.h"
#include "widgets/calendar/address_test.h"
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
  calendar::Config config = calendar::parseConfig(core::widgetSection(root, "calendar"), issues);
  if (services.calendar == nullptr) return nullptr;
  return std::make_unique<calendar::CalendarSource>(*services.calendar, std::move(config));
}

}  // namespace

const std::vector<WidgetType>& widgetTypes() {
  static const std::vector<WidgetType> types = {
      {"mvg", ui::SizeStep::kM, true, false, createMvg,
       [](const DataSource& s, ui::SizeStep step) -> std::unique_ptr<View> {
         const auto& source = static_cast<const mvg::MvgSource&>(s);
         return std::make_unique<mvg::DeparturesColumnView>(source, step);
       }},
      {"weather", ui::SizeStep::kS, true, true, createWeather,
       [](const DataSource& s, ui::SizeStep step) -> std::unique_ptr<View> {
         const auto& source = static_cast<const weather::WeatherSource&>(s);
         return std::make_unique<weather::ForecastColumnView>(source, step);
       }},
      // The calendar names its next event in the band (2026-11-16).
      {"calendar", ui::SizeStep::kS, true, false, createCalendar,
       [](const DataSource& s, ui::SizeStep step) -> std::unique_ptr<View> {
         const auto& source = static_cast<const calendar::CalendarSource&>(s);
         return std::make_unique<calendar::AgendaColumnView>(source, step);
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

bool bandEnabled(JsonObjectConst root, const WidgetType& type, core::ConfigIssues& issues) {
  const JsonObjectConst section = root["widgets"][type.id].as<JsonObjectConst>();
  if (section.isNull() || section["band"].isNull()) return true;
  const std::string path = std::string("widgets.") + type.id;
  if (!type.band) {
    issues.push_back(
        {core::configPath(path, "band"), "this widget has nothing for the band; ignored"});
    return true;
  }
  return core::readBool(section, "band", true, path, issues);
}

ui::LayoutOptions layoutOptions(const std::vector<std::string>& slots, bool dividers) {
  ui::LayoutOptions options;
  options.dividers = dividers;
  if (slots.size() == 2) {
    options.columns = 2;
    return options;
  }
  options.columns = 3;
  for (size_t i = 0; i < slots.size() && i < 3; i++) {
    const WidgetType* type = findWidgetType(slots[i]);
    if (type != nullptr && type->min_step >= ui::SizeStep::kM) {
      options.wide_column = static_cast<int>(i);
      break;
    }
  }
  return options;
}

std::string settingsLayoutJson() {
  JsonDocument doc;
  JsonArray widgets = doc["widgets"].to<JsonArray>();
  for (const WidgetType& type : widgetTypes()) {
    JsonObject w = widgets.add<JsonObject>();
    w["id"] = type.id;
    w["min"] = ui::sizeStepName(type.min_step);
    w["band"] = type.band;
    if (type.band_alone) w["band_alone"] = true;
  }
  JsonArray layouts = doc["layouts"].to<JsonArray>();
  for (int columns = ui::kMaxColumns; columns >= ui::kMinColumns; columns--) {
    ui::LayoutOptions options;
    options.columns = columns;
    JsonObject l = layouts.add<JsonObject>();
    JsonArray steps = l["steps"].to<JsonArray>();
    for (ui::SizeStep step : ui::screenLayout(options).steps) steps.add(ui::sizeStepName(step));
    if (columns == 3) l["wide"] = ui::sizeStepName(ui::SizeStep::kM);
  }
  JsonObject steps = doc["steps"].to<JsonObject>();
  for (ui::SizeStep step : {ui::SizeStep::kS, ui::SizeStep::kM, ui::SizeStep::kL}) {
    steps[ui::sizeStepName(step)] = ui::stepWidth(step);
  }
  JsonObject screen = doc["screen"].to<JsonObject>();
  screen["width"] = ui::kScreenWidth;
  screen["height"] = ui::kScreenHeight;
  screen["band"] = ui::kBriefingBandHeight;
  screen["margin"] = ui::kMargin;
  screen["gap"] = ui::kColumnGap;
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
  // "Adresse testen": the address typed in (not saved), else the saved one.
  // Never sent back and never logged (it is a secret).
  routes.push_back({"calendar/test",
                    [](const auto& args, std::string& json) {
                      std::string url = argument(args, "url");
                      if (url.empty()) {
                        core::NvsSecretStore secrets;
                        secrets.read(core::kSecretCalendarUrl, url);
                      }
                      std::string reason;
                      if (url.empty() ||
                          !core::checkSecret(core::kSecretCalendarUrl, url, reason)) {
                        json = url.empty() ? R"({"ok":false,"error":"no_address"})"
                                           : R"({"ok":false,"error":"invalid"})";
                        return true;
                      }
                      calendar::UrlSource source(url);
                      const calendar::AddressTest test =
                          calendar::testAddress(source, static_cast<int64_t>(time(nullptr)));
                      if (test.outcome == calendar::TestOutcome::kOk) {
                        core::logInfo("calendar", "address test: works, %u events in %d days",
                                      static_cast<unsigned>(test.events), calendar::kTestDays);
                      } else {
                        core::logWarning("calendar", "address test failed: %s",
                                         calendar::testOutcomeName(test.outcome));
                      }
                      json = calendar::addressTestJson(test);
                      return true;
                    },
                    true});
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
