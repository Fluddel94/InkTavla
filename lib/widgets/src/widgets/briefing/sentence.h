#pragma once

// The sentence at the top of the "Briefing" layout, built from fixed rules
// and text blocks (no AI). First version (2026-10-21): greeting by the time
// of day and today's weather only.
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
#include "widgets/weather/forecast.h"

namespace widgets::briefing {

inline constexpr int kMaybeRainPercent = 30;
inline constexpr int kRainPercent = 70;
inline constexpr int kHotDegrees = 30;

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

}  // namespace widgets::briefing
