#include "widgets/briefing/sentence.h"

#include <algorithm>
#include <cstdio>

#include "core/time.h"

namespace widgets::briefing {

namespace {

using weather::Condition;

bool sameDay(const core::LocalTime& a, const core::LocalTime& b) {
  return a.year == b.year && a.month == b.month && a.day == b.day;
}

WeatherSummary::Kind kindOf(Condition c) {
  if (c == Condition::kSnow) return WeatherSummary::Kind::kSnow;
  if (c == Condition::kThunder) return WeatherSummary::Kind::kThunder;
  return WeatherSummary::Kind::kRain;
}

bool rainy(Condition c) {
  return c == Condition::kRain || c == Condition::kDrizzle || c == Condition::kShowers;
}

std::string hourText(int hour, ui::Language language) {
  char buffer[16];
  // "14 Uhr" with a no-break space, so the line is never broken inside.
  std::snprintf(buffer, sizeof(buffer), language == ui::Language::kGerman ? "%d\u00a0Uhr" : "%d:00",
                hour);
  return buffer;
}

}  // namespace

Greeting greetingFor(int hour) {
  if (hour < 11) return Greeting::kMorning;
  if (hour < 18) return Greeting::kDay;
  return Greeting::kEvening;
}

WeatherSummary summarize(const weather::Forecast& forecast, int64_t now_s) {
  WeatherSummary s;
  core::LocalTime now;
  if (!core::isValidUnixTime(now_s) || !core::toLocalTime(now_s, now)) return s;

  const weather::Day* today = nullptr;
  for (const weather::Day& d : forecast.days) {
    core::LocalTime t;
    // Daily values start at midnight at the location; noon is safely inside.
    if (core::toLocalTime(d.time + 12 * 3600, t) && sameDay(t, now)) today = &d;
  }
  if (today == nullptr) return s;
  s.valid = true;
  s.low = weather::displayTemperature(today->low);
  s.high = weather::displayTemperature(today->high);
  s.hot = s.high >= kHotDegrees;

  // The hours still ahead today (from the current one).
  const weather::Hour* maybe = nullptr;
  const weather::Hour* likely = nullptr;
  for (const weather::Hour& h : forecast.hours) {
    core::LocalTime t;
    if (!core::toLocalTime(h.time, t) || !sameDay(t, now) || t.hour < now.hour) continue;
    if (likely == nullptr && h.rain_probability >= kRainPercent) likely = &h;
    if (maybe == nullptr && h.rain_probability >= kMaybeRainPercent) maybe = &h;
  }
  const weather::Hour* first = likely != nullptr ? likely : maybe;
  if (first != nullptr) {
    s.rain = likely != nullptr ? WeatherSummary::Rain::kLikely : WeatherSummary::Rain::kMaybe;
    s.kind = kindOf(first->condition);
    core::LocalTime t;
    core::toLocalTime(first->time, t);
    s.from_hour = t.hour <= now.hour ? -1 : t.hour;
  }

  // Picture: the umbrella (snowflake, thunderstorm) for likely rain,
  // otherwise the weather of the day; at night the moon for clear skies.
  if (s.rain == WeatherSummary::Rain::kLikely) {
    s.picture = s.kind == WeatherSummary::Kind::kSnow      ? ui::WeatherIcon::kSnow
                : s.kind == WeatherSummary::Kind::kThunder ? ui::WeatherIcon::kThunder
                                                           : ui::WeatherIcon::kRain;
  } else if (rainy(today->condition) || today->condition == Condition::kThunder) {
    s.picture = ui::WeatherIcon::kCloudy;  // no umbrella while rain is only possible
  } else {
    s.picture = weather::conditionIcon(today->condition, forecast.current.day);
  }
  return s;
}

const char* greetingText(Greeting greeting, ui::Language language) {
  const bool de = language == ui::Language::kGerman;
  switch (greeting) {
    case Greeting::kMorning:
      return de ? "Guten Morgen." : "Good morning.";
    case Greeting::kDay:
      return de ? "Guten Tag." : "Good afternoon.";
    case Greeting::kEvening:
      return de ? "Guten Abend." : "Good evening.";
  }
  return "";
}

std::string weatherText(const WeatherSummary& s, Greeting greeting, ui::Language language) {
  if (!s.valid) return "";
  const bool de = language == ui::Language::kGerman;
  const bool now = s.from_hour < 0;
  const std::string from = now ? "" : hourText(s.from_hour, language);
  std::string text;
  using Kind = WeatherSummary::Kind;
  if (s.rain == WeatherSummary::Rain::kLikely) {
    switch (s.kind) {
      case Kind::kRain:
        text = de ? (now ? "Es regnet" : "Ab " + from + " regnet es")
                  : (now ? "It is raining" : "Rain from " + from);
        text += de ? " – Schirm mit." : " – take an umbrella.";
        break;
      case Kind::kSnow:
        text = de ? (now ? "Es schneit." : "Ab " + from + " schneit es.")
                  : (now ? "It is snowing." : "Snow from " + from + ".");
        break;
      case Kind::kThunder:
        text = de ? (now ? "Gewitter" : "Ab " + from + " Gewitter")
                  : (now ? "Thunderstorms" : "Thunderstorms from " + from);
        text += de ? " – Schirm mit." : " – take an umbrella.";
        break;
    }
  } else if (s.rain == WeatherSummary::Rain::kMaybe) {
    const char* what_de = s.kind == Kind::kSnow      ? "Schnee"
                          : s.kind == Kind::kThunder ? "Gewitter"
                                                     : "Regen";
    const char* what_en = s.kind == Kind::kSnow      ? "snow"
                          : s.kind == Kind::kThunder ? "thunderstorms"
                                                     : "rain";
    if (de) {
      text = now ? std::string("Vielleicht ") + what_de + "."
                 : "Ab " + from + " vielleicht " + what_de + ".";
    } else {
      text = now ? std::string("Maybe ") + what_en + "."
                 : std::string("Maybe ") + what_en + " from " + from + ".";
    }
  } else {
    const bool evening = greeting == Greeting::kEvening;
    text = de ? (evening ? "Es bleibt trocken." : "Heute bleibt es trocken.")
              : (evening ? "It stays dry." : "Dry all day.");
  }
  if (s.hot) {
    text += de ? " Heiß, bis " : " Hot, up to ";
    text += std::to_string(s.high) + "°.";
  }
  return text;
}

std::string offlineText(const std::string& as_of, ui::Language language) {
  const bool de = language == ui::Language::kGerman;
  if (as_of.empty()) return de ? "Keine Verbindung." : "No connection.";
  return de ? "Keine Verbindung – die Angaben sind von " + as_of + "."
            : "No connection – the data is from " + as_of + ".";
}

std::string rangeText(const WeatherSummary& s, ui::Language language) {
  return std::to_string(s.low) + "° " + (language == ui::Language::kGerman ? "bis" : "to") + " " +
         std::to_string(s.high) + "°";
}

}  // namespace widgets::briefing
