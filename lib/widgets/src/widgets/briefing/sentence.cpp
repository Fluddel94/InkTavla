#include "widgets/briefing/sentence.h"

#include <algorithm>
#include <cstdio>

#include "core/time.h"

namespace widgets::briefing {

namespace {

using weather::Condition;
using weather::Forecast;
using weather::Hour;

constexpr int64_t kDay = 86400;

bool sameDay(const core::LocalTime& a, const core::LocalTime& b) {
  return a.year == b.year && a.month == b.month && a.day == b.day;
}

// "14 Uhr" with a no-break space, so the line is never broken inside.
std::string hourText(int hour, ui::Language language) {
  char buffer[16];
  std::snprintf(buffer, sizeof(buffer), language == ui::Language::kGerman ? "%d Uhr" : "%d:00",
                hour);
  return buffer;
}

std::string clockText(int64_t utc) {
  core::LocalTime t;
  char buffer[8] = "--:--";
  if (core::toLocalTime(utc, t))
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", t.hour, t.minute);
  return buffer;
}

// Daily values of the local day `offset` days after now's.
const weather::Day* dayAt(const Forecast& forecast, int64_t now_s, int offset) {
  core::LocalTime want;
  if (!core::toLocalTime(now_s + offset * kDay, want)) return nullptr;
  for (const weather::Day& d : forecast.days) {
    core::LocalTime t;
    // Daily values start at midnight at the location; noon is safely inside.
    if (core::toLocalTime(d.time + 12 * 3600, t) && sameDay(t, want)) return &d;
  }
  return nullptr;
}

// Temperature of the hour `hour` of the day of `day_time`; false if missing.
bool temperatureAt(const Forecast& forecast, int64_t day_time, int hour, int& out) {
  const std::vector<const Hour*> h = weather::hoursOfDay(forecast, day_time, hour, hour);
  if (h.empty()) return false;
  out = weather::displayTemperature(h.front()->temperature);
  return true;
}

// --- Rain ahead ---------------------------------------------------------------

enum class Wet { kNone, kMaybe, kLikely };
enum class Kind { kRain, kSnow, kThunder };

struct RainSpell {
  Wet level = Wet::kNone;
  Kind kind = Kind::kRain;
  int from = -1;        // first hour; -1: from the first hour of the span (now)
  int until = -1;       // hour the likely rain stops; -1: not within the span
  int temperature = 0;  // at the first hour
};

Kind kindOf(Condition c) {
  if (c == Condition::kSnow) return Kind::kSnow;
  if (c == Condition::kThunder) return Kind::kThunder;
  return Kind::kRain;
}

// Rain in the hours first..last of the day of day_time. The first likely
// hour counts even if a "maybe" comes earlier.
RainSpell rainSpell(const Forecast& forecast, int64_t day_time, int first, int last) {
  RainSpell s;
  const std::vector<const Hour*> hours = weather::hoursOfDay(forecast, day_time, first, last);
  const Hour* likely = nullptr;
  const Hour* maybe = nullptr;
  for (const Hour* h : hours) {
    if (likely == nullptr && h->rain_probability >= kRainPercent) likely = h;
    if (maybe == nullptr && h->rain_probability >= kMaybeRainPercent) maybe = h;
  }
  const Hour* start = likely != nullptr ? likely : maybe;
  if (start == nullptr) return s;
  s.level = likely != nullptr ? Wet::kLikely : Wet::kMaybe;
  s.kind = kindOf(start->condition);
  s.temperature = weather::displayTemperature(start->temperature);
  core::LocalTime t;
  core::toLocalTime(start->time, t);
  s.from = t.hour <= first ? -1 : t.hour;
  if (likely != nullptr) {
    bool inside = false;
    for (const Hour* h : hours) {
      if (h == likely) inside = true;
      if (inside && h->rain_probability < kRainPercent) {
        core::toLocalTime(h->time, t);
        s.until = t.hour;
        break;
      }
    }
  }
  return s;
}

// --- Wordings -------------------------------------------------------------------

struct Text {
  const char* de;
  const char* en;
};

const char* pick(const Text& t, ui::Language language) {
  return language == ui::Language::kGerman ? t.de : t.en;
}

// Replaces {hi}, {lo}, {eve}, {at} (temperatures), {from}, {until} (hours)
// and {what} / {early} / {later} (words).
struct Values {
  int hi = 0, lo = 0, eve = 0, at = 0;
  std::string from, until, what, early, later;
};

std::string fill(const std::string& text, const Values& v) {
  std::string out;
  for (size_t i = 0; i < text.size(); i++) {
    if (text[i] != '{') {
      out += text[i];
      continue;
    }
    const size_t end = text.find('}', i);
    const std::string key = text.substr(i + 1, end - i - 1);
    if (key == "hi") out += std::to_string(v.hi);
    if (key == "lo") out += std::to_string(v.lo);
    if (key == "eve") out += std::to_string(v.eve);
    if (key == "at") out += std::to_string(v.at);
    if (key == "from") out += v.from;
    if (key == "until") out += v.until;
    if (key == "what") out += v.what;
    if (key == "early") out += v.early;
    if (key == "later") out += v.later;
    i = end;
  }
  return out;
}

// Weather of a span as one of four words.
enum class Sky { kClear, kPartly, kOvercast, kFog };

Sky skyOf(Condition c) {
  switch (c) {
    case Condition::kClear:
      return Sky::kClear;
    case Condition::kMainlyClear:
    case Condition::kPartlyCloudy:
      return Sky::kPartly;
    case Condition::kFog:
      return Sky::kFog;
    default:
      return Sky::kOvercast;
  }
}

Sky skyOfSpan(const Forecast& f, int64_t day_time, int first, int last) {
  return skyOf(weather::spanCondition(weather::hoursOfDay(f, day_time, first, last)));
}

const char* skyWord(Sky sky, ui::Language language) {
  static const Text words[] = {
      {"sonnig", "sunny"}, {"wolkig", "partly cloudy"}, {"bewölkt", "cloudy"}, {"neblig", "foggy"}};
  return pick(words[static_cast<int>(sky)], language);
}

const char* precipitationWord(Kind kind, ui::Language language) {
  static const Text words[] = {
      {"Regen", "rain"}, {"Schnee", "snow"}, {"Gewitter", "thunderstorms"}};
  return pick(words[static_cast<int>(kind)], language);
}

// A wording: the sentence and its shorter form (without the temperature).
struct Wording {
  Text full;
  Text shorter;
};

// One of the wordings, chosen by the date and the time of day.
const Wording& choose(const std::vector<Wording>& list, int64_t now_s, Greeting greeting) {
  core::LocalTime t;
  core::toLocalTime(now_s, t);
  const int day = t.year * 372 + t.month * 31 + t.day;
  return list[static_cast<size_t>(day * 3 + static_cast<int>(greeting)) % list.size()];
}

// Dry days (morning: the whole day).
const std::vector<Wording>& dryDay(Sky sky, bool hot) {
  static const std::vector<Wording> clear = {
      {{"Heute wird es sonnig, bis {hi}°.", "Sunny today, up to {hi}°."},
       {"Heute wird es sonnig.", "Sunny today."}},
      {{"Ein sonniger Tag, bis {hi}°.", "A sunny day, up to {hi}°."},
       {"Ein sonniger Tag.", "A sunny day."}},
      {{"Viel Sonne heute, bis {hi}°.", "Lots of sun today, up to {hi}°."},
       {"Viel Sonne heute.", "Lots of sun today."}}};
  static const std::vector<Wording> partly = {
      {{"Heute wolkig und trocken, bis {hi}°.", "Partly cloudy and dry, up to {hi}°."},
       {"Heute wolkig und trocken.", "Partly cloudy and dry."}},
      {{"Mal Sonne, mal Wolken, bis {hi}°.", "Sun and clouds, up to {hi}°."},
       {"Mal Sonne, mal Wolken.", "Sun and clouds."}}};
  static const std::vector<Wording> overcast = {
      {{"Heute bewölkt, aber trocken, bis {hi}°.", "Cloudy but dry, up to {hi}°."},
       {"Heute bewölkt, aber trocken.", "Cloudy but dry."}},
      {{"Ein grauer, trockener Tag, bis {hi}°.", "A grey, dry day, up to {hi}°."},
       {"Ein grauer, trockener Tag.", "A grey, dry day."}}};
  static const std::vector<Wording> fog = {
      {{"Heute trüb und neblig, bis {hi}°.", "Dull and foggy, up to {hi}°."},
       {"Heute trüb und neblig.", "Dull and foggy."}}};
  static const std::vector<Wording> hot_clear = {
      {{"Sonnig und heiß, bis {hi}°.", "Sunny and hot, up to {hi}°."},
       {"Sonnig und heiß.", "Sunny and hot."}}};
  static const std::vector<Wording> hot_other = {
      {{"Heute {what} und heiß, bis {hi}°.", "{what} and hot today, up to {hi}°."},
       {"Heute {what} und heiß.", "{what} and hot today."}}};
  if (hot) return sky == Sky::kClear ? hot_clear : hot_other;
  switch (sky) {
    case Sky::kClear:
      return clear;
    case Sky::kPartly:
      return partly;
    case Sky::kFog:
      return fog;
    case Sky::kOvercast:
      break;
  }
  return overcast;
}

// Dry afternoons (11-18 h): the afternoon and the evening.
const std::vector<Wording>& dryAfternoon(Sky sky) {
  static const std::vector<Wording> clear = {
      {{"Der Nachmittag bleibt sonnig, am Abend {eve}°.",
        "A sunny afternoon, {eve}° in the evening."},
       {"Der Nachmittag bleibt sonnig.", "A sunny afternoon."}},
      {{"Sonnig bis zum Abend, dann {eve}°.", "Sunny until the evening, then {eve}°."},
       {"Sonnig bis zum Abend.", "Sunny until the evening."}}};
  static const std::vector<Wording> partly = {
      {{"Am Nachmittag wolkig, der Abend bleibt trocken bei {eve}°.",
        "Partly cloudy this afternoon, a dry evening at {eve}°."},
       {"Am Nachmittag wolkig, der Abend bleibt trocken.",
        "Partly cloudy this afternoon, a dry evening."}}};
  static const std::vector<Wording> overcast = {
      {{"Bewölkt, aber trocken, am Abend {eve}°.", "Cloudy but dry, {eve}° in the evening."},
       {"Bewölkt, aber trocken bis zum Abend.", "Cloudy but dry until the evening."}}};
  static const std::vector<Wording> fog = {
      {{"Trüb und neblig, am Abend {eve}°.", "Dull and foggy, {eve}° in the evening."},
       {"Trüb und neblig bis zum Abend.", "Dull and foggy until the evening."}}};
  switch (sky) {
    case Sky::kClear:
      return clear;
    case Sky::kPartly:
      return partly;
    case Sky::kFog:
      return fog;
    case Sky::kOvercast:
      break;
  }
  return overcast;
}

// Dry tomorrows (from 18 h).
const std::vector<Wording>& dryTomorrow(Sky sky) {
  static const std::vector<Wording> clear = {
      {{"Morgen wird es sonnig, {lo}° bis {hi}°.", "Sunny tomorrow, {lo}° to {hi}°."},
       {"Morgen wird es sonnig.", "Sunny tomorrow."}},
      {{"Morgen scheint die Sonne, {lo}° bis {hi}°.", "Sunshine tomorrow, {lo}° to {hi}°."},
       {"Morgen scheint die Sonne.", "Sunshine tomorrow."}}};
  static const std::vector<Wording> partly = {
      {{"Morgen wolkig und trocken, {lo}° bis {hi}°.",
        "Partly cloudy and dry tomorrow, {lo}° to {hi}°."},
       {"Morgen wolkig und trocken.", "Partly cloudy and dry tomorrow."}}};
  static const std::vector<Wording> overcast = {
      {{"Morgen bewölkt, aber trocken, {lo}° bis {hi}°.",
        "Cloudy but dry tomorrow, {lo}° to {hi}°."},
       {"Morgen bewölkt, aber trocken.", "Cloudy but dry tomorrow."}}};
  static const std::vector<Wording> fog = {
      {{"Morgen trüb und neblig, {lo}° bis {hi}°.", "Dull and foggy tomorrow, {lo}° to {hi}°."},
       {"Morgen trüb und neblig.", "Dull and foggy tomorrow."}}};
  switch (sky) {
    case Sky::kClear:
      return clear;
    case Sky::kPartly:
      return partly;
    case Sky::kFog:
      return fog;
    case Sky::kOvercast:
      break;
  }
  return overcast;
}

// The adjective for the start of a sentence ("Meist bewölkt").
const char* skyStart(Sky sky, ui::Language language) {
  static const Text words[] = {{"Meist sonnig", "Mostly sunny"},
                               {"Wechselnd bewölkt", "Partly cloudy"},
                               {"Meist bewölkt", "Mostly cloudy"},
                               {"Neblig", "Foggy"}};
  return pick(words[static_cast<int>(sky)], language);
}

std::string capitalized(std::string s) {
  if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = static_cast<char>(s[0] - 'a' + 'A');
  return s;
}

// Builds "full" and "shorter" from two parts: the weather and the
// temperature clause (left out in the shorter form).
void join(std::string& full, std::string& shorter, const std::string& main,
          const std::string& temperature) {
  shorter = main;
  full = temperature.empty() ? main : main + " " + temperature;
}

// --- The three times of day -------------------------------------------------------

// Before 11 h: the whole day.
void morning(const Forecast& f, int64_t now_s, const core::LocalTime& now, ui::Language lang,
             std::string& full, std::string& shorter) {
  const bool de = lang == ui::Language::kGerman;
  const weather::Day* today = dayAt(f, now_s, 0);
  if (today == nullptr) return;
  Values v;
  v.hi = weather::displayTemperature(today->high);
  v.lo = weather::displayTemperature(today->low);
  const bool hot = v.hi >= kHotDegrees;
  const RainSpell r = rainSpell(f, now_s, now.hour, 23);
  v.at = r.temperature;
  v.from = r.from >= 0 ? hourText(r.from, lang) : "";
  v.until = r.until >= 0 ? hourText(r.until, lang) : "";
  const std::string hi_text = fill(
      hot ? (de ? "Heiß, bis {hi}°." : "Hot, up to {hi}°.") : (de ? "Bis {hi}°." : "Up to {hi}°."),
      v);
  if (r.level == Wet::kLikely) {
    const bool slippery = r.kind == Kind::kSnow || r.temperature <= kSlipperyDegrees;
    const std::string glatt =
        slippery ? (de ? " Es kann glatt werden." : " It may be slippery.") : "";
    if (r.kind == Kind::kSnow) {
      const std::string main =
          r.from < 0 ? (de ? "Heute schneit es, um {at}°." : "Snow today, around {at}°.")
                     : (de ? "Ab {from} schneit es, um {at}°." : "Snow from {from}, around {at}°.");
      full = shorter = fill(main, v) + glatt;
      return;
    }
    const bool thunder = r.kind == Kind::kThunder;
    std::string main;
    if (r.from < 0 && (r.until < 0 || r.until >= 20)) {
      main = thunder ? (de ? "Heute immer wieder Gewitter – Schirm mit."
                           : "Thunderstorms all day – take an umbrella.")
                     : (de ? "Heute regnet es den ganzen Tag – Schirm mit."
                           : "Rain all day – take an umbrella.");
      join(full, shorter, fill(main, v) + glatt,
           fill(de ? "Höchstens {hi}°." : "At most {hi}°.", v));
      return;
    }
    if (r.from < 0) {
      main = thunder ? (de ? "Gewitter bis etwa {until} – Schirm mit."
                           : "Thunderstorms until about {until} – take an umbrella.")
                     : (de ? "Es regnet bis etwa {until} – Schirm mit."
                           : "Rain until about {until} – take an umbrella.");
    } else {
      main = thunder ? (de ? "Ab {from} Gewitter – Schirm mit."
                           : "Thunderstorms from {from} – take an umbrella.")
                     : (de ? "Erst trocken, ab {from} Regen – Schirm mit."
                           : "Dry at first, rain from {from} – take an umbrella.");
    }
    join(full, shorter, fill(main, v) + glatt, hi_text);
    return;
  }
  const Sky sky = skyOf(weather::dayCondition(f, *today));
  if (r.level == Wet::kMaybe) {
    v.what = precipitationWord(r.kind, lang);
    const std::string main =
        std::string(skyStart(sky, lang)) +
        (r.from < 0 ? (de ? ", vielleicht {what}." : ", maybe {what}.")
                    : (de ? ", ab {from} vielleicht {what}." : ", maybe {what} from {from}."));
    join(full, shorter, fill(main, v), hi_text);
    return;
  }
  v.what = de ? skyWord(sky, lang) : capitalized(skyWord(sky, lang));
  const Wording& w = choose(dryDay(sky, hot), now_s, Greeting::kMorning);
  full = fill(pick(w.full, lang), v);
  shorter = fill(pick(w.shorter, lang), v);
}

// 11-18 h: the afternoon and the evening.
void afternoon(const Forecast& f, int64_t now_s, const core::LocalTime& now, ui::Language lang,
               std::string& full, std::string& shorter) {
  const bool de = lang == ui::Language::kGerman;
  if (dayAt(f, now_s, 0) == nullptr) return;
  Values v;
  const bool evening = temperatureAt(f, now_s, kEveningHour, v.eve);
  const std::string eve_text =
      evening ? fill(de ? "Am Abend {eve}°." : "{eve}° in the evening.", v) : "";
  const RainSpell r = rainSpell(f, now_s, now.hour, 23);
  v.at = r.temperature;
  v.from = r.from >= 0 ? hourText(r.from, lang) : "";
  v.until = r.until >= 0 ? hourText(r.until, lang) : "";
  if (r.level == Wet::kLikely) {
    const bool slippery = r.kind == Kind::kSnow || r.temperature <= kSlipperyDegrees;
    const std::string glatt =
        slippery ? (de ? " Es kann glatt werden." : " It may be slippery.") : "";
    std::string main;
    if (r.kind == Kind::kSnow) {
      main = r.from < 0 ? (de ? "Es schneit." : "It is snowing.")
                        : (de ? "Ab {from} schneit es." : "Snow from {from}.");
    } else if (r.from < 0) {
      if (r.until < 0 || r.until >= 22) {
        main = r.kind == Kind::kThunder
                   ? (de ? "Gewitter bis in den Abend – Schirm mit."
                         : "Thunderstorms into the evening – take an umbrella.")
                   : (de ? "Es regnet bis in den Abend – Schirm mit."
                         : "Rain into the evening – take an umbrella.");
      } else {
        main = de ? "Es regnet noch bis etwa {until}, danach trocken."
                  : "Rain until about {until}, then dry.";
      }
    } else {
      main = r.kind == Kind::kThunder ? (de ? "Ab {from} Gewitter – Schirm mit."
                                            : "Thunderstorms from {from} – take an umbrella.")
                                      : (de ? "Ab {from} regnet es – Schirm mit."
                                            : "Rain from {from} – take an umbrella.");
    }
    join(full, shorter, fill(main, v) + glatt, eve_text);
    return;
  }
  if (r.level == Wet::kMaybe) {
    v.what = precipitationWord(r.kind, lang);
    const std::string main =
        r.from < 0 ? (de ? "Vielleicht {what}." : "Maybe {what}.")
                   : (de ? "Ab {from} vielleicht {what}." : "Maybe {what} from {from}.");
    join(full, shorter, capitalized(fill(main, v)), eve_text);
    return;
  }
  const Sky sky = skyOfSpan(f, now_s, now.hour, 17);
  const Wording& w = choose(dryAfternoon(sky), now_s, Greeting::kDay);
  full = fill(pick(evening ? w.full : w.shorter, lang), v);
  shorter = fill(pick(w.shorter, lang), v);
}

// From 18 h: tomorrow.
void tomorrow(const Forecast& f, int64_t now_s, ui::Language lang, std::string& full,
              std::string& shorter) {
  const bool de = lang == ui::Language::kGerman;
  const weather::Day* today = dayAt(f, now_s, 0);
  const weather::Day* next = dayAt(f, now_s, 1);
  if (next == nullptr) return;
  const int64_t day = now_s + kDay;
  Values v;
  v.hi = weather::displayTemperature(next->high);
  v.lo = weather::displayTemperature(next->low);
  const std::string hi_text = fill(de ? "Bis {hi}°." : "Up to {hi}°.", v);
  const RainSpell r = rainSpell(f, day, 6, 22);
  v.at = r.temperature;
  v.from = r.from >= 0 ? hourText(r.from, lang) : "";
  if (r.level == Wet::kLikely) {
    if (r.kind == Kind::kSnow) {
      const std::string glatt = de ? " – es kann glatt werden." : " – it may be slippery.";
      const std::string main =
          (r.from < 0 ? (de ? "Morgen früh Schnee" : "Snow tomorrow morning")
                      : (de ? "Morgen ab {from} Schnee" : "Snow tomorrow from {from}")) +
          glatt;
      join(full, shorter, fill(main, v), fill(de ? "Höchstens {hi}°." : "At most {hi}°.", v));
      return;
    }
    const bool thunder = r.kind == Kind::kThunder;
    if (r.from < 0 && (r.until < 0 || r.until >= 18)) {
      const std::string main = thunder ? (de ? "Morgen immer wieder Gewitter – Schirm einpacken."
                                             : "Thunderstorms all day tomorrow – pack an umbrella.")
                                       : (de ? "Morgen regnet es den ganzen Tag – Schirm einpacken."
                                             : "Rain all day tomorrow – pack an umbrella.");
      join(full, shorter, main, fill(de ? "Höchstens {hi}°." : "At most {hi}°.", v));
      return;
    }
    std::string main;
    if (r.from < 0 || r.from <= 7) {
      main = thunder ? (de ? "Morgen schon früh Gewitter – Schirm einpacken."
                           : "Thunderstorms early tomorrow – pack an umbrella.")
                     : (de ? "Morgen regnet es schon früh – Schirm einpacken."
                           : "Rain early tomorrow – pack an umbrella.");
    } else {
      main = thunder ? (de ? "Morgen ab {from} Gewitter – Schirm einpacken."
                           : "Thunderstorms tomorrow from {from} – pack an umbrella.")
                     : (de ? "Morgen ab {from} Regen – Schirm einpacken."
                           : "Rain tomorrow from {from} – pack an umbrella.");
    }
    join(full, shorter, fill(main, v), hi_text);
    return;
  }
  const Sky sky = skyOf(weather::dayCondition(f, *next));
  if (r.level == Wet::kMaybe) {
    v.what = precipitationWord(r.kind, lang);
    const std::string main =
        r.from < 0
            ? (de ? "Morgen vielleicht {what}" : "Maybe {what} tomorrow")
            : (de ? "Morgen ab {from} vielleicht {what}" : "Maybe {what} tomorrow from {from}");
    shorter = fill(main, v) + ".";
    full = fill(main + (de ? ", bis {hi}°." : ", up to {hi}°."), v);
    return;
  }
  // A big change of the high.
  if (today != nullptr) {
    const int change = v.hi - weather::displayTemperature(today->high);
    if (change >= kTemperatureJumpDegrees || -change >= kTemperatureJumpDegrees) {
      v.what = skyWord(sky, lang);
      const bool warmer = change > 0;
      shorter =
          de ? (warmer ? "Morgen wird es deutlich wärmer." : "Morgen wird es deutlich kälter.")
             : (warmer ? "Much warmer tomorrow." : "Much colder tomorrow.");
      full = fill(de ? (warmer ? "Morgen wird es deutlich wärmer: bis {hi}°, {what}."
                               : "Morgen wird es deutlich kälter: höchstens {hi}°, {what}.")
                     : (warmer ? "Much warmer tomorrow: up to {hi}°, {what}."
                               : "Much colder tomorrow: at most {hi}°, {what}."),
                  v);
      return;
    }
  }
  // Fog or clouds in the morning, then sun.
  const Sky early = skyOfSpan(f, day, 6, 10);
  const Sky later = skyOfSpan(f, day, 11, 17);
  if ((early == Sky::kFog || early == Sky::kOvercast) &&
      (later == Sky::kClear || later == Sky::kPartly)) {
    v.early = early == Sky::kFog ? (de ? "Nebel" : "fog") : skyWord(early, lang);
    v.later = skyWord(later, lang);
    const std::string main =
        de ? "Morgen früh {early}, später {later}" : "Tomorrow {early} at first, later {later}";
    shorter = fill(main, v) + ".";
    full = fill(main + (de ? ", bis {hi}°." : ", up to {hi}°."), v);
    return;
  }
  const Wording& w = choose(dryTomorrow(sky), now_s, Greeting::kEvening);
  full = fill(pick(w.full, lang), v);
  shorter = fill(pick(w.shorter, lang), v);
}

}  // namespace

Greeting greetingFor(int hour) {
  if (hour < 11) return Greeting::kMorning;
  if (hour < 18) return Greeting::kDay;
  return Greeting::kEvening;
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

WeatherSummary summarize(const Forecast& forecast, int64_t now_s) {
  WeatherSummary s;
  core::LocalTime now;
  if (!core::isValidUnixTime(now_s) || !core::toLocalTime(now_s, now)) return s;
  const weather::Day* today = dayAt(forecast, now_s, 0);
  if (today == nullptr) return s;
  s.valid = true;
  s.current = weather::displayTemperature(forecast.current.temperature);

  // Picture: the umbrella (snowflake, thunderstorm) for likely rain in the
  // hours still ahead today, otherwise the weather of the day (by its
  // hours); in the evening the weather of now; at night the moon for clear
  // skies.
  const RainSpell r = rainSpell(forecast, now_s, now.hour, 23);
  if (r.level == Wet::kLikely) {
    s.picture = r.kind == Kind::kSnow      ? ui::WeatherIcon::kSnow
                : r.kind == Kind::kThunder ? ui::WeatherIcon::kThunder
                                           : ui::WeatherIcon::kRain;
    return s;
  }
  Condition c = weather::dayCondition(forecast, *today);
  if (greetingFor(now.hour) == Greeting::kEvening &&
      forecast.current.condition != Condition::kUnknown) {
    c = forecast.current.condition;
  }
  // No umbrella while rain is only possible or over: a cloud.
  if (c == Condition::kRain || c == Condition::kDrizzle || c == Condition::kShowers ||
      c == Condition::kThunder) {
    c = Condition::kOvercast;
  }
  s.picture = weather::conditionIcon(c, forecast.current.day);
  return s;
}

std::string nowText(const WeatherSummary& summary, ui::Language language) {
  return (language == ui::Language::kGerman ? "Jetzt " : "Now ") + std::to_string(summary.current) +
         "°";
}

std::string weatherSentence(const Forecast& forecast, int64_t now_s, ui::Language language,
                            std::string* shorter) {
  core::LocalTime now;
  std::string full, short_form;
  if (core::isValidUnixTime(now_s) && core::toLocalTime(now_s, now)) {
    switch (greetingFor(now.hour)) {
      case Greeting::kMorning:
        morning(forecast, now_s, now, language, full, short_form);
        break;
      case Greeting::kDay:
        afternoon(forecast, now_s, now, language, full, short_form);
        break;
      case Greeting::kEvening:
        tomorrow(forecast, now_s, language, full, short_form);
        break;
    }
  }
  if (shorter != nullptr) *shorter = short_form == full ? "" : short_form;
  return full;
}

std::string offlineText(const std::string& as_of, ui::Language language) {
  const bool de = language == ui::Language::kGerman;
  if (as_of.empty()) return de ? "Keine Verbindung." : "No connection.";
  return de ? "Keine Verbindung – die Angaben sind von " + as_of + "."
            : "No connection – the data is from " + as_of + ".";
}

std::string cancellationText(const std::vector<mvg::Section>& sections, int64_t now_s,
                             ui::Language language) {
  if (!core::isValidUnixTime(now_s)) return "";
  // The next departures of all sections together.
  std::vector<const mvg::Departure*> next;
  for (const mvg::Section& s : sections) {
    for (const mvg::Departure& d : s.departures) {
      if (d.planned >= now_s) next.push_back(&d);
    }
  }
  std::stable_sort(next.begin(), next.end(), [](const mvg::Departure* a, const mvg::Departure* b) {
    return a->planned < b->planned;
  });
  if (next.size() > static_cast<size_t>(kCancellationDepartures)) {
    next.resize(kCancellationDepartures);
  }
  const mvg::Departure* first = nullptr;
  for (const mvg::Departure* d : next) {
    if (d->cancelled && d->planned <= now_s + kCancellationAheadS) {
      first = d;
      break;
    }
  }
  if (first == nullptr) return "";
  const std::string time = clockText(first->planned);
  if (language == ui::Language::kGerman) {
    return "Die " + first->line + " um " + time + " nach " + first->destination + " fällt aus.";
  }
  return "The " + first->line + " at " + time + " to " + first->destination + " is cancelled.";
}

std::vector<Part> sentenceParts(const SentenceInput& in) {
  std::vector<Part> parts;
  if (!core::isValidUnixTime(in.now_s)) return parts;
  if (!in.online) parts.push_back({Topic::kOffline, offlineText(in.as_of, in.language), ""});
  if (in.departures != nullptr) {
    const std::string text = cancellationText(*in.departures, in.now_s, in.language);
    if (!text.empty()) parts.push_back({Topic::kCancellation, text, ""});
  }
  if (in.forecast != nullptr) {
    std::string shorter;
    const std::string text = weatherSentence(*in.forecast, in.now_s, in.language, &shorter);
    if (!text.empty()) parts.push_back({Topic::kWeather, text, shorter});
  }
  return parts;
}

std::string joinSentence(const std::string& greeting, const std::vector<Part>& parts) {
  std::string text = greeting;
  for (const Part& p : parts) {
    if (!text.empty()) text += " ";
    text += p.text;
  }
  return text;
}

}  // namespace widgets::briefing
