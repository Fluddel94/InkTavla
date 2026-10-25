#include "widgets/weather/setup.h"

#include <cmath>

#include "core/url.h"

namespace widgets::weather {

namespace {

double round4(double v) { return std::round(v * 10000.0) / 10000.0; }

}  // namespace

std::string placeSearchUrl(const std::string& query, const std::string& language) {
  return "https://geocoding-api.open-meteo.com/v1/search?name=" + core::urlEncode(query) +
         "&count=8&language=" + (language == "en" ? "en" : "de") + "&format=json";
}

JsonDocument placeSearchFilter() {
  JsonDocument f;
  JsonObject r = f["results"][0].to<JsonObject>();
  for (const char* key : {"name", "latitude", "longitude", "country", "admin1", "admin3"}) {
    r[key] = true;
  }
  return f;
}

std::string placesJson(JsonObjectConst response) {
  JsonDocument out;
  JsonArray list = out.to<JsonArray>();
  for (JsonObjectConst r : response["results"].as<JsonArrayConst>()) {
    if (!r["latitude"].is<double>() || !r["longitude"].is<double>()) continue;
    JsonObject p = list.add<JsonObject>();
    p["name"] = r["name"] | "";
    std::string region = r["admin1"] | "";
    const std::string district = r["admin3"] | "";
    if (!district.empty()) region += (region.empty() ? "" : ", ") + district;
    p["region"] = region;
    p["country"] = r["country"] | "";
    p["lat"] = round4(r["latitude"].as<double>());
    p["lon"] = round4(r["longitude"].as<double>());
  }
  std::string text;
  serializeJson(out, text);
  return text;
}

}  // namespace widgets::weather
