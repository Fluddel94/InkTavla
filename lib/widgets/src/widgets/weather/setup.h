#pragma once

// Help for setting up the weather widget on the settings page: the place
// search of Open-Meteo (https://open-meteo.com/en/docs/geocoding-api, free,
// no key), so the user can type a name instead of coordinates.

#include <ArduinoJson.h>

#include <string>

namespace widgets::weather {

// https://geocoding-api.open-meteo.com/v1/search?name=<urlencoded>&count=8
//   &language=<de|en>&format=json
std::string placeSearchUrl(const std::string& query, const std::string& language);

JsonDocument placeSearchFilter();

// Places of a search response as JSON: [{"name": "Holzkirchen", "region":
// "Bayern, Landkreis Miesbach", "country": "Deutschland", "lat": 47.8766,
// "lon": 11.7018}]; coordinates with 4 decimals.
std::string placesJson(JsonObjectConst response);

}  // namespace widgets::weather
