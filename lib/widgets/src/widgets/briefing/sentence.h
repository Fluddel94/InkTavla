#pragma once

// The sentence at the top of the "Briefing" layout, built from fixed rules
// and text blocks (no AI): a greeting by the time of day, then (2026-10-28)
//  1. no Wi-Fi ("Keine Verbindung zum WLAN – die Angaben sind von 07:12.",
//     shorter "Kein WLAN – Stand 07:12."), or, with Wi-Fi, widgets whose
//     updates fail ("Die Abfahrten lassen sich gerade nicht abrufen – Stand
//     07:12.", shorter without the time)
//  2. a cancelled departure among the next kCancellationDepartures of each
//     section (direction, station) within the next hour, at most one ("Die
//     S2 um 07:54 nach Dachau fällt aus.")
//  3. always a sentence about the weather, by the time of day:
//     - before kMorningEndHour the whole day ("Heute wird es sonnig, bis
//       16°."),
//     - then the afternoon and the evening ("Ab 16 Uhr regnet es – Schirm
//       mit. Am Abend 9°."); after sunset the evening ("Ein klarer,
//       trockener Abend, später 5°."),
//     - from eveningStartHour() tomorrow ("Morgen früh Nebel, später
//       sonnig, bis 12°.").
// The parts stand side by side; the rank decides their order and which part
// gives way when the sentence does not fit. Disruption messages and events
// are in their columns, not in the sentence. Sources without fresh data
// leave their parts out.
//
// Sentence and symbols never contradict each other (2026-10-28):
// - today: the sky words come from the same weather as the band picture
//   (bandCondition()), rain from the same rule (rainAhead()): "Schirm mit"
//   exactly when the picture is the umbrella (a probability of kRainPercent
//   or more in one of the hours ahead), "vielleicht" from kMaybeRainPercent
//   or when the picture would be rain by the amounts;
// - tomorrow: the weather word comes from the daily code of Open-Meteo, as
//   the symbol of the weather column; the hours give the times ("ab 10 Uhr").
//   "Schirm einpacken" only if the daily code is wet; weather of the code
//   that the daylight hours do not show is "zeitweise".
// Of several wordings for the same weather the date and the time of day pick
// one, so it stays the same until the next change of the time of day.

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
// "Am Abend 9°": the temperature at this hour (or the next hour, if later).
inline constexpr int kEveningHour = 20;
// Times of day of the weather sentence: the whole day before this hour.
inline constexpr int kMorningEndHour = 11;
// Tomorrow from the first full hour after sunset (the first hour of today
// that Open-Meteo marks as night), but never before or after these hours
// (2026-10-28). Without hours of today: kEveningEarliestHour in winter time,
// kEveningSummerHour in summer time.
inline constexpr int kEveningEarliestHour = 18;
inline constexpr int kEveningLatestHour = 21;
inline constexpr int kEveningSummerHour = 19;
// Snow, or rain at or below this temperature: "Es kann glatt werden."
inline constexpr int kSlipperyDegrees = 1;

enum class Greeting { kMorning, kDay, kEvening };

// Before 11 h morning, before 18 h day, then evening (the greeting only; the
// weather sentence follows weatherPeriod()).
Greeting greetingFor(int hour);

// Hour from which the weather sentence is about tomorrow (see above).
int eveningStartHour(const weather::Forecast& forecast, int64_t now_s);

// Time of day of the weather sentence: kMorning before kMorningEndHour,
// kEvening from eveningStartHour(), else kDay.
Greeting weatherPeriod(const weather::Forecast& forecast, int64_t now_s);

const char* greetingText(Greeting greeting, ui::Language language);

// What the picture of the band shows: by daylight the weather of the rest
// of the daylight hours, at night the weather of now; the umbrella
// (snowflake, thunderstorm) for likely rain in the hours ahead today, a
// cloud for rain that is only possible.
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

// "Keine Verbindung zum WLAN – die Angaben sind von 07:12." (as_of = "" if
// unknown).
std::string offlineText(const std::string& as_of, ui::Language language);

// Widgets of the screen that the band can name when their updates fail.
enum class Source { kWeather, kDepartures, kCalendar };

struct FailedSource {
  Source source;
  std::string as_of;  // time of its last data ("" if it never had any)
};

// "Das Wetter lässt sich gerade nicht abrufen – Stand 07:12." for one
// source, "Wetter und Abfahrten lassen sich gerade nicht abrufen." for
// several; "" for none.
std::string failureText(const std::vector<FailedSource>& failed, ui::Language language);

// "Die S2 um 07:54 nach Dachau fällt aus." if one of the next
// kCancellationDepartures departures of a section (from now on; each
// direction or station for itself, these are always among the rows the
// column draws) is cancelled and leaves within kCancellationAheadS; the
// earliest such one. "" if none.
std::string cancellationText(const std::vector<mvg::Section>& sections, int64_t now_s,
                             ui::Language language);

// --- The whole sentence -----------------------------------------------------

// Topics of the parts; the order is the rank and the reading order.
enum class Topic {
  kOffline,
  kFailure,
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
  // Widgets whose updates have failed for a while (only said when online).
  std::vector<FailedSource> failed;
  const weather::Forecast* forecast = nullptr;
  const std::vector<mvg::Section>* departures = nullptr;
};

// The parts that apply, ordered by rank.
std::vector<Part> sentenceParts(const SentenceInput& input);

// Greeting and the parts, in order.
std::string joinSentence(const std::string& greeting, const std::vector<Part>& parts);

}  // namespace widgets::briefing
