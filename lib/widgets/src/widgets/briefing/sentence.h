#pragma once

// The sentence at the top of the "Briefing" layout, built from fixed rules
// and text blocks (no AI): a greeting by the time of day, then (2026-10-26)
//  1. no connection ("Keine Verbindung – die Angaben sind von 07:12.")
//  2. a cancelled departure among the next kCancellationDepartures of the
//     shown lines within the next hour, at most one ("Die S2 um 07:54 nach
//     Dachau fällt aus.")
//  3. always a sentence about the weather, by the time of day:
//     - before 11 h the whole day ("Heute wird es sonnig, bis 16°."),
//     - 11-18 h the afternoon and the evening ("Ab 16 Uhr regnet es –
//       Schirm mit. Am Abend 9°."),
//     - from 18 h tomorrow ("Morgen früh Nebel, später sonnig, bis 12°.").
// Disruption messages and events are not in the sentence any more; they are
// in their columns. Sources without fresh data leave their parts out.
//
// Rules for rain (snow, thunderstorms) in the weather sentence, for the
// hours still ahead: a probability of kRainPercent or more is rain (with
// "Schirm mit"), kMaybeRainPercent-69 % "vielleicht", less is dry. The
// weather words (sunny, cloudy, foggy) follow the rules of the day symbol
// (weather::spanCondition()). Of several wordings for the same weather the
// date and the time of day pick one, so it stays the same until the next
// change of the time of day.

#include <cstdint>
#include <string>
#include <vector>

#include "ui/i18n.h"
#include "ui/icons.h"
#include "widgets/mvg/departure.h"
#include "widgets/weather/forecast.h"

namespace widgets::briefing {

inline constexpr int kMaybeRainPercent = 30;
inline constexpr int kRainPercent = 70;
inline constexpr int kHotDegrees = 30;
// Difference of tomorrow's high to today's that is mentioned in the evening.
inline constexpr int kTemperatureJumpDegrees = 8;
// Cancellations: only among the next departures, up to this long ahead.
inline constexpr int kCancellationDepartures = 2;
inline constexpr int64_t kCancellationAheadS = 3600;
// "Am Abend 9°": the temperature at this hour.
inline constexpr int kEveningHour = 20;
// Snow, or rain at or below this temperature: "Es kann glatt werden."
inline constexpr int kSlipperyDegrees = 1;

enum class Greeting { kMorning, kDay, kEvening };

// Before 11 h morning, before 18 h day, then evening.
Greeting greetingFor(int hour);

const char* greetingText(Greeting greeting, ui::Language language);

// What the picture of the band shows: the weather of now (today's weather
// during the day, the current one in the evening), the umbrella (snowflake,
// thunderstorm) for likely rain today.
struct WeatherSummary {
  bool valid = false;  // today's values are in the forecast
  int current = 0;     // temperature now (rounded)
  ui::WeatherIcon picture = ui::WeatherIcon::kSun;
};

// Today's weather from the forecast as of now_s (local time of the device).
WeatherSummary summarize(const weather::Forecast& forecast, int64_t now_s);

// "Jetzt 8°" / "Now 8°" below the picture.
std::string nowText(const WeatherSummary& summary, ui::Language language);

// The weather sentence for the time of day of now_s (see above); "" without
// values for the day it is about. *shorter (if given) gets a shorter form
// without the temperature, for when the whole sentence does not fit.
std::string weatherSentence(const weather::Forecast& forecast, int64_t now_s, ui::Language language,
                            std::string* shorter = nullptr);

// "Keine Verbindung – die Angaben sind von 07:12." (as_of = "" if unknown).
std::string offlineText(const std::string& as_of, ui::Language language);

// "Die S2 um 07:54 nach Dachau fällt aus." if one of the next
// kCancellationDepartures departures (from now on, all sections together)
// is cancelled and leaves within kCancellationAheadS; the first such one.
// "" if none.
std::string cancellationText(const std::vector<mvg::Section>& sections, int64_t now_s,
                             ui::Language language);

// --- The whole sentence -----------------------------------------------------

// Topics of the parts; the order is the rank and the reading order.
enum class Topic {
  kOffline,
  kCancellation,
  kWeather,
};

struct Part {
  Topic topic;
  std::string text;
  std::string shorter;  // shorter form, "" = none
};

// What the sentence is built from; nullptr for a source without fresh data.
struct SentenceInput {
  int64_t now_s = 0;
  ui::Language language = ui::Language::kGerman;
  bool online = true;
  std::string as_of;  // time of the last weather data for the offline part
  const weather::Forecast* forecast = nullptr;
  const std::vector<mvg::Section>* departures = nullptr;
};

// The parts that apply, ordered by rank.
std::vector<Part> sentenceParts(const SentenceInput& input);

// Greeting and the parts, in order.
std::string joinSentence(const std::string& greeting, const std::vector<Part>& parts);

}  // namespace widgets::briefing
