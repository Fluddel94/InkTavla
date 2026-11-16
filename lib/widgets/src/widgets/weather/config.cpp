#include "widgets/weather/config.h"

#include <cstdio>

namespace widgets::weather {

namespace {

constexpr char kPath[] = "widgets.weather";

// A number within [min, max]; false (with an issue) if set but invalid.
bool readCoordinate(JsonObjectConst section, const char* key, double min, double max, double& out,
                    core::ConfigIssues& issues) {
  JsonVariantConst value = section[key];
  if (value.isNull()) return false;
  if (!value.is<double>() || value.as<double>() < min || value.as<double>() > max) {
    char range[64];
    std::snprintf(range, sizeof(range), "must be a number between %g and %g", min, max);
    issues.push_back({core::configPath(kPath, key), range});
    return false;
  }
  out = value.as<double>();
  return true;
}

}  // namespace

Config parseConfig(JsonObjectConst section, core::ConfigIssues& issues) {
  Config c;
  if (section.isNull()) return c;
  core::reportUnknownKeys(section, {"latitude", "longitude", "name", "band"}, kPath, issues);
  const bool lat = readCoordinate(section, "latitude", -90, 90, c.latitude, issues);
  const bool lon = readCoordinate(section, "longitude", -180, 180, c.longitude, issues);
  c.has_location = lat && lon;
  if (section["latitude"].isNull() != section["longitude"].isNull()) {
    issues.push_back({kPath, "needs both latitude and longitude"});
  }
  c.name = core::readString(section, "name", "", 32, kPath, issues);
  return c;
}

}  // namespace widgets::weather
