#include "widgets/briefing/sentence.h"

#include <algorithm>
#include <cstdio>

#include "core/time.h"
#include "widgets/mvg/departures.h"

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

// Daily values of the local day `offset` days after now's.
const weather::Day* dayAt(const weather::Forecast& forecast, int64_t now_s, int offset) {
  core::LocalTime want;
  if (!core::toLocalTime(now_s + offset * 86400, want)) return nullptr;
  for (const weather::Day& d : forecast.days) {
    core::LocalTime t;
    if (core::toLocalTime(d.time + 12 * 3600, t) && sameDay(t, want)) return &d;
  }
  return nullptr;
}

std::string clockText(int64_t utc) {
  core::LocalTime t;
  char buffer[8] = "--:--";
  if (core::toLocalTime(utc, t))
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", t.hour, t.minute);
  return buffer;
}

// At most max_chars characters (UTF-8), else shortened with "…".
std::string shorten(const std::string& text, size_t max_chars) {
  size_t chars = 0;
  for (size_t i = 0; i < text.length(); i++) {
    if ((static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) continue;
    if (chars == max_chars) {
      std::string cut = text.substr(0, i);
      while (!cut.empty() &&
             (cut.back() == ' ' || cut.back() == ',' || cut.back() == '.' || cut.back() == '-')) {
        cut.pop_back();
      }
      return cut + "\u2026";
    }
    chars++;
  }
  return text;
}

std::string listText(const std::vector<std::string>& items, const char* and_word) {
  std::string text;
  for (size_t i = 0; i < items.size(); i++) {
    if (i > 0) text += i + 1 == items.size() ? std::string(" ") + and_word + " " : ", ";
    text += items[i];
  }
  return text;
}

// Reading order of the topics.
int readingRank(Topic topic) {
  switch (topic) {
    case Topic::kOffline:
      return 0;
    case Topic::kCancellation:
      return 1;
    case Topic::kDisruption:
      return 2;
    case Topic::kWeather:
      return 3;
    case Topic::kCalm:
      return 4;
    case Topic::kTemperatureJump:
      return 5;
    case Topic::kEvent:
      return 6;
  }
  return 7;
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
    // In the evening the weather of now rather than of the whole day (the
    // daily code reports the worst weather, maybe of the morning).
    Condition c = today->condition;
    if (greetingFor(now.hour) == Greeting::kEvening &&
        forecast.current.condition != Condition::kUnknown) {
      c = forecast.current.condition;
      if (rainy(c) || c == Condition::kThunder) c = Condition::kOvercast;
    }
    s.picture = weather::conditionIcon(c, forecast.current.day);
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

bool isCalm(const WeatherSummary& s) {
  return s.valid && s.rain == WeatherSummary::Rain::kNone && !s.hot;
}

std::string temperatureJumpText(const weather::Forecast& forecast, int64_t now_s,
                                ui::Language language) {
  core::LocalTime now;
  if (!core::isValidUnixTime(now_s) || !core::toLocalTime(now_s, now)) return "";
  if (greetingFor(now.hour) != Greeting::kEvening) return "";
  const weather::Day* today = dayAt(forecast, now_s, 0);
  const weather::Day* tomorrow = dayAt(forecast, now_s, 1);
  if (today == nullptr || tomorrow == nullptr) return "";
  const int high = weather::displayTemperature(tomorrow->high);
  const int change = high - weather::displayTemperature(today->high);
  if (change < kTemperatureJumpDegrees && -change < kTemperatureJumpDegrees) return "";
  const bool de = language == ui::Language::kGerman;
  const std::string degrees = std::to_string(high) + "°.";
  if (change > 0) {
    return de ? "Morgen wird es deutlich wärmer, bis " + degrees
              : "Tomorrow much warmer, up to " + degrees;
  }
  return de ? "Morgen wird es deutlich kälter, höchstens " + degrees
            : "Tomorrow much colder, at most " + degrees;
}

std::string cancellationText(const std::vector<mvg::Section>& sections, int64_t now_s,
                             ui::Language language) {
  if (!core::isValidUnixTime(now_s)) return "";
  const mvg::Departure* first = nullptr;
  for (const mvg::Section& s : sections) {
    for (const mvg::Departure& d : s.departures) {
      if (!d.cancelled || d.planned < now_s || d.planned > now_s + kCancellationAheadS) continue;
      if (first == nullptr || d.planned < first->planned) first = &d;
    }
  }
  if (first == nullptr) return "";
  const std::string time = clockText(first->planned);
  if (language == ui::Language::kGerman) {
    return "Die " + first->line + " um\u00a0" + time + " nach " + first->destination +
           " fällt aus.";
  }
  return "The " + first->line + " at\u00a0" + time + " to " + first->destination + " is cancelled.";
}

std::string disruptionText(const std::vector<mvg::Incident>& incidents, ui::Language language) {
  if (incidents.empty()) return "";
  const bool de = language == ui::Language::kGerman;
  const std::vector<std::string> lines = mvg::incidentLines(incidents);
  std::string text;
  if (lines.size() == 1) {
    text = (de ? "Störung bei der " : "Disruption on the ") + lines[0];
  } else if (lines.empty() || lines.size() > 3) {
    text = de ? "Störungen bei mehreren Linien" : "Disruptions on several lines";
  } else {
    text = (de ? "Störungen bei " : "Disruptions on ") + listText(lines, de ? "und" : "and");
  }
  return text + (de ? " – plane mehr Zeit ein." : " – allow extra time.");
}

std::string eventText(const calendar::Occurrences& events, int64_t now_s, ui::Language language) {
  core::LocalTime now;
  if (!core::isValidUnixTime(now_s) || !core::toLocalTime(now_s, now)) return "";
  const bool de = language == ui::Language::kGerman;
  const int64_t today = calendar::localDay(now_s);
  const calendar::Occurrence* next = nullptr;
  const calendar::Occurrence* all_day = nullptr;
  for (const calendar::Occurrence& o : events) {
    if (o.all_day) {
      if (all_day == nullptr && o.first_day <= today && o.last_day >= today) all_day = &o;
    } else if (o.first_day == today && o.start >= now_s) {
      if (next == nullptr || o.start < next->start) next = &o;
    }
  }
  const auto title = [](const calendar::Occurrence& o) {
    return shorten(std::string(o.summary.data(), o.summary.size()), kEventTitleChars);
  };
  if (next != nullptr) {
    return (de ? "Um\u00a0" + clockText(next->start) + " "
               : "At\u00a0" + clockText(next->start) + ": ") +
           title(*next) + ".";
  }
  if (greetingFor(now.hour) == Greeting::kEvening) {
    // Tomorrow's first event: all-day events come first.
    for (const calendar::Occurrence& o : events) {
      if (o.first_day > today + 1 || o.last_day < today + 1) continue;
      if (!o.all_day && o.first_day != today + 1) continue;
      if (o.all_day) return (de ? "Morgen: " : "Tomorrow: ") + title(o) + ".";
      return (de ? "Morgen um\u00a0" + clockText(o.start) + " "
                 : "Tomorrow at\u00a0" + clockText(o.start) + ": ") +
             title(o) + ".";
    }
    return "";
  }
  if (all_day != nullptr) return (de ? "Heute: " : "Today: ") + title(*all_day) + ".";
  return "";
}

std::vector<Part> sentenceParts(const SentenceInput& in) {
  std::vector<Part> parts;
  core::LocalTime now;
  if (!core::isValidUnixTime(in.now_s) || !core::toLocalTime(in.now_s, now)) return parts;
  const Greeting greeting = greetingFor(now.hour);
  const auto add = [&parts](Topic topic, const std::string& text) {
    if (!text.empty()) parts.push_back({topic, text});
  };
  if (!in.online) add(Topic::kOffline, offlineText(in.as_of, in.language));
  if (in.departures != nullptr) {
    add(Topic::kCancellation, cancellationText(*in.departures, in.now_s, in.language));
  }
  if (in.incidents != nullptr) add(Topic::kDisruption, disruptionText(*in.incidents, in.language));
  if (in.forecast != nullptr) {
    const WeatherSummary s = summarize(*in.forecast, in.now_s);
    add(isCalm(s) ? Topic::kCalm : Topic::kWeather, weatherText(s, greeting, in.language));
    add(Topic::kTemperatureJump, temperatureJumpText(*in.forecast, in.now_s, in.language));
  }
  if (in.events != nullptr) add(Topic::kEvent, eventText(*in.events, in.now_s, in.language));
  std::stable_sort(parts.begin(), parts.end(), [](const Part& a, const Part& b) {
    return static_cast<int>(a.topic) < static_cast<int>(b.topic);
  });
  // The calm part only when there is nothing else to say about the day.
  if (parts.size() > 1 && parts.back().topic == Topic::kCalm) {
    bool other = false;
    for (const Part& p : parts) {
      other = other || (p.topic != Topic::kCalm && p.topic != Topic::kOffline);
    }
    if (other) parts.pop_back();
  }
  return parts;
}

std::string joinSentence(const std::string& greeting, const std::vector<Part>& parts,
                         size_t count) {
  std::vector<Part> kept(parts.begin(), parts.begin() + std::min(count, parts.size()));
  std::stable_sort(kept.begin(), kept.end(), [](const Part& a, const Part& b) {
    return readingRank(a.topic) < readingRank(b.topic);
  });
  std::string text = greeting;
  for (const Part& p : kept) {
    if (!text.empty()) text += " ";
    text += p.text;
  }
  return text;
}

}  // namespace widgets::briefing
