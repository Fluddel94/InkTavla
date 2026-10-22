#pragma once

// The sentence at the top of the "Briefing" layout, built from fixed rules
// and text blocks (no AI): a greeting by the time of day, then parts on
// different topics. By rank (the lowest is left out first when the sentence
// does not fit into three lines, see band.h):
//  1. no connection ("Keine Verbindung – die Angaben sind von 07:12.")
//  2. a cancelled departure of the shown lines in the next hour ("Die S6 um
//     07:54 nach Ebersberg fällt aus.")
//  3. disruption messages of the shown lines ("Störung bei der S3 – plane
//     mehr Zeit ein.")
//  4. rain, snow, thunderstorms or heat today (rules below)
//  5. in the evening a big change of tomorrow's high ("Morgen wird es
//     deutlich kälter, höchstens 6°.")
//  6. the next event (today; in the evening also tomorrow's first)
//  7. the calm part when nothing else is to say ("Heute bleibt es trocken.")
// Sources without fresh data leave their parts out. Severe weather warnings
// (DWD) are not available yet.
//
// Rules for the weather part, for the hours still ahead today:
//  - rain probability of 70 % or more: rain (snow, thunderstorm) "from 14 h"
//    or "now", with "Schirm mit" for rain and thunderstorms;
//  - 30-69 %: "vielleicht Regen" (snow, thunderstorm);
//  - below 30 %: it stays dry;
//  - a high of 30° or more: "Heiß, bis 32°".

#include <cstdint>
#include <string>

#include "ui/i18n.h"
#include "ui/icons.h"
#include "widgets/calendar/agenda.h"
#include "widgets/mvg/departure.h"
#include "widgets/weather/forecast.h"

namespace widgets::briefing {

inline constexpr int kMaybeRainPercent = 30;
inline constexpr int kRainPercent = 70;
inline constexpr int kHotDegrees = 30;
// Difference of tomorrow's high to today's that is mentioned in the evening.
inline constexpr int kTemperatureJumpDegrees = 8;
// Cancellations are mentioned up to this long before the departure.
inline constexpr int64_t kCancellationAheadS = 3600;
// Event titles in the sentence are shortened to this many characters.
inline constexpr size_t kEventTitleChars = 32;

enum class Greeting { kMorning, kDay, kEvening };

// Before 11 h morning, before 18 h day, then evening.
Greeting greetingFor(int hour);

struct WeatherSummary {
  bool valid = false;  // today's values are in the forecast
  int low = 0;         // today's low and high (rounded)
  int high = 0;
  enum class Rain { kNone, kMaybe, kLikely } rain = Rain::kNone;
  enum class Kind { kRain, kSnow, kThunder } kind = Kind::kRain;
  int from_hour = -1;  // first hour of the rain; -1: it starts now
  bool hot = false;
  ui::WeatherIcon picture = ui::WeatherIcon::kSun;  // for the band
};

// Today's weather from the forecast as of now_s (local time of the device).
WeatherSummary summarize(const weather::Forecast& forecast, int64_t now_s);

// The weather part of the sentence ("Ab 14 Uhr regnet es – Schirm mit.").
std::string weatherText(const WeatherSummary& summary, Greeting greeting, ui::Language language);

const char* greetingText(Greeting greeting, ui::Language language);

// "Keine Verbindung – die Angaben sind von 07:12." (as_of = "" if unknown).
std::string offlineText(const std::string& as_of, ui::Language language);

// "7° bis 14°" / "7° to 14°".
std::string rangeText(const WeatherSummary& summary, ui::Language language);

// True if the weather part is the calm one (dry, not hot).
bool isCalm(const WeatherSummary& summary);

// "Morgen wird es deutlich wärmer, bis 22°." in the evening if tomorrow's
// high differs from today's by kTemperatureJumpDegrees or more; else "".
std::string temperatureJumpText(const weather::Forecast& forecast, int64_t now_s,
                                ui::Language language);

// "Die S6 um 07:54 nach Ebersberg fällt aus." for the first cancelled
// departure within kCancellationAheadS; "" if none.
std::string cancellationText(const std::vector<mvg::Section>& sections, int64_t now_s,
                             ui::Language language);

// "Störung bei der S3 – plane mehr Zeit ein." / "Störungen bei S3 und S8 –
// …"; "" without messages.
std::string disruptionText(const std::vector<mvg::Incident>& incidents, ui::Language language);

// The next event that has not started yet today ("Um 09:30 Zahnarzt."),
// else an all-day event of today ("Heute: Geburtstag Oma."); in the evening
// tomorrow's first event ("Morgen um 08:00 Zahnarzt."). "" if none.
std::string eventText(const calendar::Occurrences& events, int64_t now_s, ui::Language language);

// --- The whole sentence -----------------------------------------------------

// Topics of the parts; the order is the rank (kOffline first).
enum class Topic {
  kOffline,
  kCancellation,
  kDisruption,
  kWeather,
  kTemperatureJump,
  kEvent,
  kCalm,
};

struct Part {
  Topic topic;
  std::string text;
};

// What the sentence is built from; nullptr for a source without fresh data.
struct SentenceInput {
  int64_t now_s = 0;
  ui::Language language = ui::Language::kGerman;
  bool online = true;
  std::string as_of;  // time of the last weather data for the offline part
  const weather::Forecast* forecast = nullptr;
  const std::vector<mvg::Section>* departures = nullptr;
  const std::vector<mvg::Incident>* incidents = nullptr;
  const calendar::Occurrences* events = nullptr;
};

// The parts that apply, ordered by rank.
std::vector<Part> sentenceParts(const SentenceInput& input);

// Greeting and the first `count` parts by rank, in reading order (offline,
// departures, weather, the calm part, tomorrow's temperature, the event).
std::string joinSentence(const std::string& greeting, const std::vector<Part>& parts, size_t count);

}  // namespace widgets::briefing
