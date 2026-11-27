#include "widgets/briefing/sentence.h"

#include <algorithm>
#include <cstdio>

#include "core/time.h"
#include "ui/fonts.h"
#include "ui/text.h"

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

bool isWet(Condition c) {
  return c == Condition::kRain || c == Condition::kDrizzle || c == Condition::kShowers ||
         c == Condition::kSnow || c == Condition::kThunder;
}

// A symbol can show rain, snow or a thunderstorm (by the amounts of the
// hours, or by the daily code) while no hour reaches even the probability
// for "maybe". A dry sentence would contradict the symbol, so the first wet
// hour of the span makes it a "maybe" (never "likely": that stays with the
// probability).
RainSpell followSymbol(const Forecast& forecast, int64_t day_time, int first, int last,
                       Condition symbol, RainSpell spell) {
  if (spell.level != Wet::kNone || !isWet(symbol)) return spell;
  for (const Hour* h : weather::hoursOfDay(forecast, day_time, first, last)) {
    const bool thunder = symbol == Condition::kThunder && h->condition == Condition::kThunder;
    if (h->precipitation < weather::kRainHourMm - 0.001f && !thunder) continue;
    RainSpell s;
    s.level = Wet::kMaybe;
    s.kind = kindOf(symbol);
    s.temperature = weather::displayTemperature(h->temperature);
    core::LocalTime t;
    core::toLocalTime(h->time, t);
    s.from = t.hour <= first ? -1 : t.hour;
    return s;
  }
  return spell;
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
// and {what} / {later} (words).
struct Values {
  int hi = 0, lo = 0, eve = 0, at = 0;
  std::string from, until, what, later;
};

std::string fill(const std::string& text, const Values& v) {
  std::string out;
  for (size_t i = 0; i < text.size(); i++) {
    if (text[i] != '{') {
      out += text[i];
      continue;
    }
    const size_t end = text.find('}', i);
    if (end == std::string::npos) {  // no closing brace: keep the rest as it is
      out += text.substr(i);
      break;
    }
    const std::string key = text.substr(i + 1, end - i - 1);
    if (key == "hi") out += std::to_string(v.hi);
    if (key == "lo") out += std::to_string(v.lo);
    if (key == "eve") out += std::to_string(v.eve);
    if (key == "at") out += std::to_string(v.at);
    if (key == "from") out += v.from;
    if (key == "until") out += v.until;
    if (key == "what") out += v.what;
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

// Weather of the daylight hours of today from `hour` on (the whole day if
// none is left); the morning sentence talks about it (before sunrise the
// band picture shows the night, after sunrise the same hours).
Condition remainingCondition(const Forecast& f, int64_t now_s, int hour) {
  std::vector<const Hour*> hours = weather::daylightHours(f, now_s, hour);
  if (hours.empty()) hours = weather::daylightHours(f, now_s);
  return weather::spanCondition(hours);
}

// The weather of the band picture, also the sky of the afternoon and evening
// sentence: by daylight (current.is_day) the rest of the daylight hours, at
// night (or without daylight hours left) the weather of now; the daily code
// if neither is known.
Condition bandCondition(const Forecast& f, int64_t now_s, int hour, const weather::Day& today) {
  Condition c = f.current.condition;
  if (f.current.day) {
    const std::vector<const Hour*> rest = weather::daylightHours(f, now_s, hour);
    if (!rest.empty()) c = weather::spanCondition(rest);
  }
  if (c == Condition::kUnknown) c = weather::dayCondition(f, today);
  return c;
}

// Rain in the hours ahead today, for the band picture and the sentence: the
// same rule for both (see sentence.h).
RainSpell rainAhead(const Forecast& f, int64_t now_s, int hour, Condition symbol) {
  return followSymbol(f, now_s, hour, 23, symbol, rainSpell(f, now_s, hour, 23));
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

// Index of one of `count` wordings, chosen by the date and the time of day:
// the next one every day (2026-11-16: "day * 3 + greeting" chose the same
// one every day for lists of three).
size_t chooseIndex(size_t count, int64_t now_s, Greeting greeting) {
  core::LocalTime t;
  core::toLocalTime(now_s, t);
  const int day = t.year * 372 + t.month * 31 + t.day;
  return static_cast<size_t>(day + static_cast<int>(greeting)) % count;
}

const Wording& choose(const std::vector<Wording>& list, int64_t now_s, Greeting greeting) {
  return list[chooseIndex(list.size(), now_s, greeting)];
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
       {"Mal Sonne, mal Wolken.", "Sun and clouds."}},
      {{"Ein trockener Tag mit Sonne und Wolken, bis {hi}°.",
        "A dry day with sun and clouds, up to {hi}°."},
       {"Ein trockener Tag mit Sonne und Wolken.", "A dry day with sun and clouds."}}};
  static const std::vector<Wording> overcast = {
      {{"Heute bewölkt, aber trocken, bis {hi}°.", "Cloudy but dry, up to {hi}°."},
       {"Heute bewölkt, aber trocken.", "Cloudy but dry."}},
      {{"Ein grauer, trockener Tag, bis {hi}°.", "A grey, dry day, up to {hi}°."},
       {"Ein grauer, trockener Tag.", "A grey, dry day."}},
      {{"Viele Wolken, aber kein Regen, bis {hi}°.", "Lots of clouds but no rain, up to {hi}°."},
       {"Viele Wolken, aber kein Regen.", "Lots of clouds but no rain."}}};
  static const std::vector<Wording> fog = {
      {{"Heute trüb und neblig, bis {hi}°.", "Dull and foggy, up to {hi}°."},
       {"Heute trüb und neblig.", "Dull and foggy."}},
      {{"Ein nebliger Tag, bis {hi}°.", "A foggy day, up to {hi}°."},
       {"Ein nebliger Tag.", "A foggy day."}},
      {{"Heute viel Nebel, aber trocken, bis {hi}°.", "Lots of fog but dry, up to {hi}°."},
       {"Heute viel Nebel, aber trocken.", "Lots of fog but dry."}}};
  static const std::vector<Wording> hot_clear = {
      {{"Sonnig und heiß, bis {hi}°.", "Sunny and hot, up to {hi}°."},
       {"Sonnig und heiß.", "Sunny and hot."}},
      {{"Ein heißer Sommertag, bis {hi}°.", "A hot summer day, up to {hi}°."},
       {"Ein heißer Sommertag.", "A hot summer day."}}};
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

// Dry afternoons (by daylight): the afternoon and the evening.
const std::vector<Wording>& dryAfternoon(Sky sky) {
  static const std::vector<Wording> clear = {
      {{"Der Nachmittag bleibt sonnig, am Abend {eve}°.",
        "A sunny afternoon, {eve}° in the evening."},
       {"Der Nachmittag bleibt sonnig.", "A sunny afternoon."}},
      {{"Sonnig bis zum Abend, dann {eve}°.", "Sunny until the evening, then {eve}°."},
       {"Sonnig bis zum Abend.", "Sunny until the evening."}},
      {{"Weiter sonnig und trocken, am Abend {eve}°.",
        "Still sunny and dry, {eve}° in the evening."},
       {"Weiter sonnig und trocken.", "Still sunny and dry."}}};
  static const std::vector<Wording> partly = {
      {{"Am Nachmittag wolkig, der Abend bleibt trocken bei {eve}°.",
        "Partly cloudy this afternoon, a dry evening at {eve}°."},
       {"Am Nachmittag wolkig, der Abend bleibt trocken.",
        "Partly cloudy this afternoon, a dry evening."}},
      {{"Weiter mal Sonne, mal Wolken, am Abend {eve}°.",
        "Still sun and clouds, {eve}° in the evening."},
       {"Weiter mal Sonne, mal Wolken.", "Still sun and clouds."}},
      {{"Wolkig, aber trocken bis zum Abend, dann {eve}°.",
        "Partly cloudy but dry until the evening, then {eve}°."},
       {"Wolkig, aber trocken bis zum Abend.", "Partly cloudy but dry until the evening."}}};
  static const std::vector<Wording> overcast = {
      {{"Bewölkt, aber trocken, am Abend {eve}°.", "Cloudy but dry, {eve}° in the evening."},
       {"Bewölkt, aber trocken bis zum Abend.", "Cloudy but dry until the evening."}},
      {{"Der Nachmittag bleibt grau und trocken, am Abend {eve}°.",
        "A grey, dry afternoon, {eve}° in the evening."},
       {"Der Nachmittag bleibt grau und trocken.", "A grey, dry afternoon."}},
      {{"Viele Wolken, aber kein Regen, am Abend {eve}°.",
        "Lots of clouds but no rain, {eve}° in the evening."},
       {"Viele Wolken, aber kein Regen.", "Lots of clouds but no rain."}}};
  static const std::vector<Wording> fog = {
      {{"Trüb und neblig, am Abend {eve}°.", "Dull and foggy, {eve}° in the evening."},
       {"Trüb und neblig bis zum Abend.", "Dull and foggy until the evening."}},
      {{"Der Nebel bleibt, am Abend {eve}°.", "The fog stays, {eve}° in the evening."},
       {"Der Nebel bleibt bis zum Abend.", "The fog stays until the evening."}}};
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

// Dry evenings after sunset (before the sentence turns to tomorrow): the sky
// of now, as the band picture.
const std::vector<Wording>& dryEvening(Sky sky) {
  static const std::vector<Wording> clear = {
      {{"Ein klarer, trockener Abend, später {eve}°.", "A clear, dry evening, {eve}° later."},
       {"Ein klarer, trockener Abend.", "A clear, dry evening."}},
      {{"Der Abend bleibt trocken und klar, später {eve}°.",
        "The evening stays dry and clear, {eve}° later."},
       {"Der Abend bleibt trocken und klar.", "The evening stays dry and clear."}},
      {{"Klarer Himmel heute Abend, später {eve}°.", "Clear skies tonight, {eve}° later."},
       {"Klarer Himmel heute Abend.", "Clear skies tonight."}}};
  static const std::vector<Wording> partly = {
      {{"Ein trockener Abend mit ein paar Wolken, später {eve}°.",
        "A dry evening with a few clouds, {eve}° later."},
       {"Ein trockener Abend mit ein paar Wolken.", "A dry evening with a few clouds."}},
      {{"Der Abend bleibt trocken, später {eve}°.", "The evening stays dry, {eve}° later."},
       {"Der Abend bleibt trocken.", "The evening stays dry."}}};
  static const std::vector<Wording> overcast = {
      {{"Der Abend bleibt trocken, später {eve}°.", "The evening stays dry, {eve}° later."},
       {"Der Abend bleibt trocken.", "The evening stays dry."}},
      {{"Ein bewölkter, trockener Abend, später {eve}°.", "A cloudy, dry evening, {eve}° later."},
       {"Ein bewölkter, trockener Abend.", "A cloudy, dry evening."}}};
  static const std::vector<Wording> fog = {
      {{"Neblig am Abend, später {eve}°.", "A foggy evening, {eve}° later."},
       {"Neblig am Abend.", "A foggy evening."}},
      {{"Der Abend wird trüb und neblig, später {eve}°.", "A dull, foggy evening, {eve}° later."},
       {"Der Abend wird trüb und neblig.", "A dull, foggy evening."}}};
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

// Dry tomorrows (from eveningStartHour()), by the daily code.
const std::vector<Wording>& dryTomorrow(Sky sky) {
  static const std::vector<Wording> clear = {
      {{"Morgen wird es sonnig, {lo}° bis {hi}°.", "Sunny tomorrow, {lo}° to {hi}°."},
       {"Morgen wird es sonnig.", "Sunny tomorrow."}},
      {{"Morgen scheint die Sonne, {lo}° bis {hi}°.", "Sunshine tomorrow, {lo}° to {hi}°."},
       {"Morgen scheint die Sonne.", "Sunshine tomorrow."}},
      {{"Morgen ein sonniger Tag, {lo}° bis {hi}°.", "A sunny day tomorrow, {lo}° to {hi}°."},
       {"Morgen ein sonniger Tag.", "A sunny day tomorrow."}}};
  static const std::vector<Wording> partly = {
      {{"Morgen wolkig und trocken, {lo}° bis {hi}°.",
        "Partly cloudy and dry tomorrow, {lo}° to {hi}°."},
       {"Morgen wolkig und trocken.", "Partly cloudy and dry tomorrow."}},
      {{"Morgen mal Sonne, mal Wolken, {lo}° bis {hi}°.",
        "Sun and clouds tomorrow, {lo}° to {hi}°."},
       {"Morgen mal Sonne, mal Wolken.", "Sun and clouds tomorrow."}},
      {{"Morgen ein trockener Tag mit Sonne und Wolken, bis {hi}°.",
        "A dry day with sun and clouds tomorrow, up to {hi}°."},
       {"Morgen ein trockener Tag mit Sonne und Wolken.",
        "A dry day with sun and clouds tomorrow."}}};
  static const std::vector<Wording> overcast = {
      {{"Morgen bewölkt, aber trocken, {lo}° bis {hi}°.",
        "Cloudy but dry tomorrow, {lo}° to {hi}°."},
       {"Morgen bewölkt, aber trocken.", "Cloudy but dry tomorrow."}},
      {{"Morgen ein grauer, trockener Tag, {lo}° bis {hi}°.",
        "A grey, dry day tomorrow, {lo}° to {hi}°."},
       {"Morgen ein grauer, trockener Tag.", "A grey, dry day tomorrow."}},
      {{"Morgen viele Wolken, aber kein Regen, bis {hi}°.",
        "Lots of clouds but no rain tomorrow, up to {hi}°."},
       {"Morgen viele Wolken, aber kein Regen.", "Lots of clouds but no rain tomorrow."}}};
  static const std::vector<Wording> fog = {
      {{"Morgen trüb und neblig, {lo}° bis {hi}°.", "Dull and foggy tomorrow, {lo}° to {hi}°."},
       {"Morgen trüb und neblig.", "Dull and foggy tomorrow."}},
      {{"Morgen ein nebliger Tag, {lo}° bis {hi}°.", "A foggy day tomorrow, {lo}° to {hi}°."},
       {"Morgen ein nebliger Tag.", "A foggy day tomorrow."}}};
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

// The warning of ice joins the weather sentence instead of being a sentence
// of its own: "Es schneit – es kann glatt werden." (2026-11-26). A sentence
// that already has a dash ("… – Schirm mit.") gets a comma instead of a
// second dash.
std::string withSlippery(std::string sentence, bool de) {
  if (!sentence.empty() && sentence.back() == '.') sentence.pop_back();
  const bool has_dash = sentence.find(" – ") != std::string::npos;
  if (has_dash) return sentence + (de ? ", es kann glatt werden." : ", and it may be slippery.");
  return sentence + (de ? " – es kann glatt werden." : " – it may be slippery.");
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
  const RainSpell r = rainAhead(f, now_s, now.hour, remainingCondition(f, now_s, now.hour));
  v.at = r.temperature;
  v.from = r.from >= 0 ? hourText(r.from, lang) : "";
  v.until = r.until >= 0 ? hourText(r.until, lang) : "";
  const std::string hi_text = fill(
      hot ? (de ? "Heiß, bis {hi}°." : "Hot, up to {hi}°.") : (de ? "Bis {hi}°." : "Up to {hi}°."),
      v);
  if (r.level == Wet::kLikely) {
    const bool slippery = r.kind == Kind::kSnow || r.temperature <= kSlipperyDegrees;
    const auto weatherSentence = [&](const std::string& text) {
      const std::string sentence = fill(text, v);
      return slippery ? withSlippery(sentence, de) : sentence;
    };
    if (r.kind == Kind::kSnow) {
      const std::string main =
          r.from < 0 ? (de ? "Heute schneit es, um {at}°." : "Snow today, around {at}°.")
                     : (de ? "Ab {from} schneit es, um {at}°." : "Snow from {from}, around {at}°.");
      full = shorter = weatherSentence(main);
      return;
    }
    const bool thunder = r.kind == Kind::kThunder;
    std::string main;
    if (r.from < 0 && (r.until < 0 || r.until >= 20)) {
      main = thunder ? (de ? "Heute immer wieder Gewitter – Schirm mit."
                           : "Thunderstorms all day – take an umbrella.")
                     : (de ? "Heute regnet es den ganzen Tag – Schirm mit."
                           : "Rain all day – take an umbrella.");
      join(full, shorter, weatherSentence(main),
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
    join(full, shorter, weatherSentence(main), hi_text);
    return;
  }
  const Sky sky = skyOf(remainingCondition(f, now_s, now.hour));
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

// From kMorningEndHour to eveningStartHour(): the afternoon and the evening.
// The sky is the one of the band picture: the rest of the daylight hours, or
// after sunset the weather of now ("Ein klarer, trockener Abend").
void afternoon(const Forecast& f, int64_t now_s, const core::LocalTime& now, ui::Language lang,
               std::string& full, std::string& shorter) {
  const bool de = lang == ui::Language::kGerman;
  const weather::Day* today = dayAt(f, now_s, 0);
  if (today == nullptr) return;
  Values v;
  const int eve_hour = std::min(23, std::max(kEveningHour, now.hour + 1));
  const bool evening = temperatureAt(f, now_s, eve_hour, v.eve);
  const std::string eve_text =
      evening ? fill(de ? "Am Abend {eve}°." : "{eve}° in the evening.", v) : "";
  const Condition sky_now = bandCondition(f, now_s, now.hour, *today);
  const RainSpell r = rainAhead(f, now_s, now.hour, sky_now);
  v.at = r.temperature;
  v.from = r.from >= 0 ? hourText(r.from, lang) : "";
  v.until = r.until >= 0 ? hourText(r.until, lang) : "";
  if (r.level == Wet::kLikely) {
    const bool slippery = r.kind == Kind::kSnow || r.temperature <= kSlipperyDegrees;
    const auto weatherSentence = [&](const std::string& text) {
      const std::string sentence = fill(text, v);
      return slippery ? withSlippery(sentence, de) : sentence;
    };
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
    join(full, shorter, weatherSentence(main), eve_text);
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
  const Sky sky = skyOf(sky_now);
  const Wording& w =
      choose(f.current.day ? dryAfternoon(sky) : dryEvening(sky), now_s, Greeting::kDay);
  full = fill(pick(evening ? w.full : w.shorter, lang), v);
  shorter = fill(pick(w.shorter, lang), v);
}

// From eveningStartHour(): tomorrow. The weather word is the daily code (the
// symbol of the weather column); the hours give the times.
void tomorrow(const Forecast& f, int64_t now_s, ui::Language lang, std::string& full,
              std::string& shorter) {
  const bool de = lang == ui::Language::kGerman;
  const weather::Day* today = dayAt(f, now_s, 0);
  const weather::Day* next = dayAt(f, now_s, 1);
  if (next == nullptr) return;
  const int64_t day = now_s + kDay;
  const Condition code = next->condition;
  const bool wet_code = isWet(code);
  Values v;
  v.hi = weather::displayTemperature(next->high);
  v.lo = weather::displayTemperature(next->low);
  const std::string hi_text = fill(de ? "Bis {hi}°." : "Up to {hi}°.", v);
  RainSpell r = followSymbol(f, day, 6, 22, code, rainSpell(f, day, 6, 22));
  // "Schirm einpacken" only if the column shows rain (snow, a thunderstorm).
  if (r.level == Wet::kLikely && !wet_code) {
    r.level = Wet::kMaybe;
    r.until = -1;
  }
  if (r.level != Wet::kNone && wet_code) r.kind = kindOf(code);
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
  // The daily code is wet, but not in the hours from 6 to 22 h (at night).
  if (wet_code) {
    v.what = precipitationWord(kindOf(code), lang);
    const std::string main =
        de ? "Morgen zeitweise {what}, sonst trocken" : "Tomorrow {what} at times, otherwise dry";
    shorter = fill(main, v) + ".";
    full = fill(main + (de ? ", bis {hi}°." : ", up to {hi}°."), v);
    return;
  }
  const Sky sky = skyOf(code);
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
  if (sky == Sky::kFog) {
    // Fog in the morning, then sun or clouds.
    const Sky early = skyOfSpan(f, day, 6, 10);
    const Sky later = skyOfSpan(f, day, 11, 17);
    if (early == Sky::kFog && (later == Sky::kClear || later == Sky::kPartly)) {
      v.later = skyWord(later, lang);
      const std::string main =
          de ? "Morgen früh Nebel, später {later}" : "Tomorrow fog at first, later {later}";
      shorter = fill(main, v) + ".";
      full = fill(main + (de ? ", bis {hi}°." : ", up to {hi}°."), v);
      return;
    }
    // Fog of the code that the daylight hours do not show (at night).
    const std::vector<const Hour*> light = weather::daylightHours(f, day);
    if (!light.empty() && skyOf(weather::spanCondition(light)) != Sky::kFog) {
      const std::string main =
          de ? "Morgen zeitweise neblig, sonst trocken" : "Tomorrow foggy at times, otherwise dry";
      shorter = fill(main, v) + ".";
      full = fill(main + (de ? ", bis {hi}°." : ", up to {hi}°."), v);
      return;
    }
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

int eveningStartHour(const Forecast& forecast, int64_t now_s) {
  core::LocalTime now;
  if (!core::toLocalTime(now_s, now)) return kEveningEarliestHour;
  const std::vector<const Hour*> hours = weather::hoursOfDay(forecast, now_s, 12, 23);
  if (hours.empty()) return now.dst ? kEveningSummerHour : kEveningEarliestHour;
  int hour = kEveningLatestHour;  // no night hour before midnight (polar day)
  for (const Hour* h : hours) {
    if (h->day) continue;
    core::LocalTime t;
    core::toLocalTime(h->time, t);
    hour = t.hour;
    break;
  }
  return std::max(kEveningEarliestHour, std::min(kEveningLatestHour, hour));
}

Greeting weatherPeriod(const Forecast& forecast, int64_t now_s) {
  core::LocalTime now;
  if (!core::toLocalTime(now_s, now)) return Greeting::kMorning;
  if (now.hour < kMorningEndHour) return Greeting::kMorning;
  if (now.hour < eveningStartHour(forecast, now_s)) return Greeting::kDay;
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
  // hours still ahead today; otherwise by daylight the weather of the rest of
  // the daylight hours, at night the weather of now (the moon for clear
  // skies). The sentence uses the same two rules.
  //
  // W1 (2026-11-22): the umbrella with two, four or six drops and light or
  // heavy snow by the strongest code of the likely hours, the thunderstorm
  // with hail if one of them has hail. From gusts of kStormGustKmh in the
  // hours ahead today: the windsock instead of a dry sky, the windsock with
  // driven rain instead of the umbrella, the snowflake with wind lines; a
  // thunderstorm stays.
  const std::vector<const Hour*> ahead = weather::hoursOfDay(forecast, now_s, now.hour, 23);
  const bool storm = weather::maxGust(ahead) >= weather::kStormGustKmh;
  Condition c = bandCondition(forecast, now_s, now.hour, *today);
  const RainSpell r = rainAhead(forecast, now_s, now.hour, c);
  if (r.level == Wet::kLikely) {
    int strength = 0;
    bool hail = false;
    for (const Hour* h : ahead) {
      if (h->rain_probability < kRainPercent) continue;
      hail = hail || weather::isHailCode(h->code);
      if (weather::isSnowCode(h->code) == (r.kind == Kind::kSnow)) {
        strength = std::max(strength, weather::precipitationStrength(h->code));
      }
    }
    if (r.kind == Kind::kThunder) {
      s.picture = hail ? ui::WeatherIcon::kThunderHail : ui::WeatherIcon::kThunder;
    } else if (r.kind == Kind::kSnow) {
      s.picture = storm           ? ui::WeatherIcon::kSnowWind
                  : strength == 3 ? ui::WeatherIcon::kSnowHeavy
                                  : ui::WeatherIcon::kSnow;
    } else {
      s.picture = storm           ? ui::WeatherIcon::kStormRain
                  : strength == 1 ? ui::WeatherIcon::kRainLight
                  : strength == 3 ? ui::WeatherIcon::kRainHeavy
                                  : ui::WeatherIcon::kRain;
    }
    return s;
  }
  if (storm) {
    s.picture = ui::WeatherIcon::kWindSock;
    return s;
  }
  // No umbrella while rain is only possible or over: a cloud.
  if (c == Condition::kRain || c == Condition::kDrizzle || c == Condition::kShowers ||
      c == Condition::kThunder) {
    c = Condition::kOvercast;
  }
  s.picture = weather::conditionIcon(c, forecast.current.day);
  return s;
}

std::string nowText(const WeatherSummary& summary, ui::Language /*language*/) {
  return std::to_string(summary.current) + "°";
}

std::string windText(int gust, bool tomorrow, ui::Language language) {
  const bool de = language == ui::Language::kGerman;
  if (gust >= weather::kHurricaneGustKmh) {
    return tomorrow ? (de ? "Morgen orkanartige Böen möglich." : "Hurricane-force gusts tomorrow.")
                    : (de ? "Achtung, orkanartige Böen." : "Beware of hurricane-force gusts.");
  }
  if (gust >= weather::kStormGustKmh) {
    return tomorrow ? (de ? "Morgen stürmisch, Sturmböen möglich."
                          : "Stormy tomorrow, gale-force gusts possible.")
                    : (de ? "Stürmisch, Sturmböen möglich." : "Stormy, gale-force gusts possible.");
  }
  if (gust >= weather::kWindyGustKmh) {
    return tomorrow ? (de ? "Morgen windig mit kräftigen Böen." : "Windy tomorrow, strong gusts.")
                    : (de ? "Dazu windig mit kräftigen Böen." : "Windy, with strong gusts.");
  }
  return "";
}

namespace {

void replaceAll(std::string& text, const std::string& from, const std::string& to) {
  for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) {
    text.replace(at, from.size(), to);
  }
}

// The addition of the longer weather sentence (2026-11-24): a look at the
// next time of day. In the morning the temperature of the evening, in the
// afternoon the sentence of tomorrow, in the evening the low of the night
// (up to 7 h of tomorrow). "" if the forecast has no values for it.
std::string lookAhead(const Forecast& f, int64_t now_s, Greeting period, ui::Language lang) {
  const bool de = lang == ui::Language::kGerman;
  Values v;
  switch (period) {
    case Greeting::kMorning:
      if (!temperatureAt(f, now_s, kEveningHour, v.eve)) return "";
      return fill(de ? "Am Abend noch\u00a0{eve}°." : "Still {eve}° in the evening.", v);
    case Greeting::kDay: {
      std::string full, shorter;
      tomorrow(f, now_s, lang, full, shorter);
      return full;
    }
    case Greeting::kEvening: {
      const weather::Day* next = dayAt(f, now_s, 1);
      if (next == nullptr) return "";
      bool any = false;
      float low = 0;
      for (const Hour& h : f.hours) {
        if (h.time < now_s || h.time > next->time + 7 * 3600) continue;
        if (!any || h.temperature < low) low = h.temperature;
        any = true;
      }
      if (!any) return "";
      v.lo = weather::displayTemperature(low);
      return fill(de ? "Heute Nacht bis\u00a0{lo}°." : "Down to {lo}° tonight.", v);
    }
  }
  return "";
}

// From the storm threshold an umbrella is of little use (W1).
void hoodInsteadOfUmbrella(std::string& text) {
  replaceAll(text, "– Schirm mit.", "– Kapuze statt Schirm.");
  replaceAll(text, "– Schirm einpacken.", "– Kapuze statt Schirm.");
  replaceAll(text, "– take an umbrella.", "– a hood, not an umbrella.");
  replaceAll(text, "– pack an umbrella.", "– a hood, not an umbrella.");
}

}  // namespace

std::string weatherSentence(const Forecast& forecast, int64_t now_s, ui::Language language,
                            std::string* shorter, std::string* longer) {
  core::LocalTime now;
  std::string full, short_form, long_form;
  if (core::isValidUnixTime(now_s) && core::toLocalTime(now_s, now)) {
    const Greeting period = weatherPeriod(forecast, now_s);
    switch (period) {
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
    // Wind (W1): the gusts of the hours ahead today, in the evening those of
    // tomorrow from kWindFirstHour to kWindLastHour. From kStormGustKmh also
    // in the shorter form, below only in the full one.
    const bool for_tomorrow = period == Greeting::kEvening;
    const weather::Day* day = dayAt(forecast, now_s, for_tomorrow ? 1 : 0);
    if (!full.empty() && day != nullptr) {
      const int gust =
          weather::maxGust(for_tomorrow ? weather::hoursOfDay(forecast, day->time + 12 * 3600,
                                                              kWindFirstHour, kWindLastHour)
                                        : weather::hoursOfDay(forecast, now_s, now.hour, 23));
      const std::string wind = windText(gust, for_tomorrow, language);
      if (gust >= weather::kStormGustKmh) {
        hoodInsteadOfUmbrella(full);
        hoodInsteadOfUmbrella(short_form);
      }
      if (!wind.empty()) {
        // Right after the weather, before the temperature clause that the
        // shorter form leaves out ("… Schirm. Stürmisch, … Am Abend 11°.").
        if (full.compare(0, short_form.size(), short_form) == 0 && full != short_form) {
          full.insert(short_form.size(), " " + wind);
        } else {
          full += " " + wind;
        }
        if (gust >= weather::kStormGustKmh) short_form += " " + wind;
      }
    }
    if (!full.empty() && longer != nullptr) {
      const std::string more = lookAhead(forecast, now_s, period, language);
      if (!more.empty()) long_form = full + " " + more;
    }
  }
  if (shorter != nullptr) *shorter = short_form == full ? "" : short_form;
  if (longer != nullptr) *longer = long_form;
  return full;
}

std::string offlineText(const std::string& as_of, ui::Language language) {
  const bool de = language == ui::Language::kGerman;
  if (as_of.empty()) return de ? "Keine Verbindung zum WLAN." : "No Wi-Fi connection.";
  return de ? "Keine Verbindung zum WLAN – die Angaben sind von " + as_of + "."
            : "No Wi-Fi connection – the data is from " + as_of + ".";
}

std::string failureText(const std::vector<FailedSource>& failed, ui::Language language) {
  const bool de = language == ui::Language::kGerman;
  if (failed.empty()) return "";
  if (failed.size() == 1) {
    const FailedSource& f = failed.front();
    if (f.settings) {
      static const Text check[] = {
          {"Das Wetter lässt sich nicht abrufen – bitte die Einstellungen prüfen",
           "The weather cannot be loaded – please check the settings"},
          {"Die Abfahrten lassen sich nicht abrufen – bitte die Einstellungen prüfen",
           "The departures cannot be loaded – please check the settings"},
          {"Der Kalender lässt sich nicht abrufen – bitte die Adresse prüfen",
           "The calendar cannot be loaded – please check its address"}};
      return std::string(pick(check[static_cast<int>(f.source)], language)) + ".";
    }
    static const Text one[] = {
        {"Das Wetter lässt sich gerade nicht abrufen", "The weather cannot be loaded right now"},
        {"Die Abfahrten lassen sich gerade nicht abrufen",
         "The departures cannot be loaded right now"},
        {"Der Kalender lässt sich gerade nicht abrufen",
         "The calendar cannot be loaded right now"}};
    std::string text = pick(one[static_cast<int>(f.source)], language);
    if (!f.as_of.empty()) text += (de ? " – Stand " : " – as of ") + f.as_of;
    return text + ".";
  }
  static const Text names[] = {
      {"Wetter", "weather"}, {"Abfahrten", "departures"}, {"Kalender", "calendar"}};
  std::string list;
  for (size_t i = 0; i < failed.size(); i++) {
    if (i > 0) list += i + 1 == failed.size() ? (de ? " und " : " and ") : ", ";
    list += pick(names[static_cast<int>(failed[i].source)], language);
  }
  return de ? list + " lassen sich gerade nicht abrufen."
            : capitalized(list) + " cannot be loaded right now.";
}

std::string cancellationText(const std::vector<mvg::Section>& sections, int64_t now_s,
                             ui::Language language, std::string* shorter, std::string* longer) {
  if (shorter != nullptr) shorter->clear();
  if (longer != nullptr) longer->clear();
  if (!core::isValidUnixTime(now_s)) return "";
  // The next departures of each section (direction, station) for itself;
  // of the cancelled ones within the hour the earliest.
  const mvg::Departure* first = nullptr;
  const mvg::Section* section = nullptr;
  for (const mvg::Section& s : sections) {
    std::vector<const mvg::Departure*> next;
    for (const mvg::Departure& d : s.departures) {
      if (d.planned >= now_s) next.push_back(&d);
    }
    std::stable_sort(
        next.begin(), next.end(),
        [](const mvg::Departure* a, const mvg::Departure* b) { return a->planned < b->planned; });
    if (next.size() > static_cast<size_t>(kCancellationDepartures)) {
      next.resize(kCancellationDepartures);
    }
    for (const mvg::Departure* d : next) {
      if (!d->cancelled || d->planned > now_s + kCancellationAheadS) continue;
      if (first == nullptr || d->planned < first->planned) {
        first = d;
        section = &s;
      }
    }
  }
  if (first == nullptr) return "";
  const std::string time = clockText(first->planned);
  const bool de = language == ui::Language::kGerman;
  // The next departure of the same line to the same destination.
  const mvg::Departure* next = nullptr;
  for (const mvg::Departure& d : section->departures) {
    if (d.cancelled || d.planned <= first->planned || d.line != first->line ||
        d.destination != first->destination) {
      continue;
    }
    if (next == nullptr || d.planned < next->planned) next = &d;
  }
  if (shorter != nullptr) {
    *shorter = de ? first->line + " um\u00a0" + time + " fällt aus."
                  : first->line + " at\u00a0" + time + " cancelled.";
  }
  const std::string main =
      de ? "Die " + first->line + " um\u00a0" + time + " nach " + first->destination + " fällt aus"
         : "The " + first->line + " at\u00a0" + time + " to " + first->destination +
               " is cancelled";
  if (longer != nullptr && next != nullptr) {
    *longer = main + (de ? ", die nächste fährt um\u00a0" : ", the next one leaves at\u00a0") +
              clockText(next->planned) + ".";
  }
  return main + ".";
}

namespace {

// Width the title keeps in the shorter form of the event.
constexpr int kEventTitleShortWidth = 260;

// Ends with a full stop, "!", "?" or the "…" of a shortened title.
bool endsSentence(const std::string& text) {
  const std::string ellipsis = "…";
  if (text.size() >= ellipsis.size() &&
      text.compare(text.size() - ellipsis.size(), ellipsis.size(), ellipsis) == 0) {
    return true;
  }
  return !text.empty() && (text.back() == '.' || text.back() == '!' || text.back() == '?');
}

// Wordings of the event (2026-11-16, V1), chosen like the weather sentence;
// without the full stop (added unless the title ends a sentence). A no-break
// space joins "um"/"at" and the time.
const std::vector<Text>& eventWordings(bool timed, bool tomorrow) {
  static const std::vector<Text> today_timed = {
      {"Um\u00a0{time} {title}", "{title} at\u00a0{time}"},
      {"Heute um\u00a0{time}: {title}", "Next up at\u00a0{time}: {title}"},
      {"Als Nächstes um\u00a0{time}: {title}", "Coming up at\u00a0{time}: {title}"}};
  static const std::vector<Text> tomorrow_timed = {
      {"Morgen um\u00a0{time} {title}", "Tomorrow at\u00a0{time}: {title}"},
      {"Morgen als Erstes um\u00a0{time}: {title}", "{title} tomorrow at\u00a0{time}"},
      {"Für morgen vorgemerkt: {time} {title}", "First thing tomorrow, {time}: {title}"}};
  static const std::vector<Text> today_all_day = {
      {"Heute: {title}", "Today: {title}"},
      {"Heute ganztägig: {title}", "All day today: {title}"},
      {"Den ganzen Tag: {title}", "On the calendar today: {title}"}};
  static const std::vector<Text> tomorrow_all_day = {
      {"Morgen: {title}", "Tomorrow: {title}"},
      {"Morgen ganztägig: {title}", "All day tomorrow: {title}"},
      {"Morgen den ganzen Tag: {title}", "Coming up tomorrow: {title}"}};
  if (timed) return tomorrow ? tomorrow_timed : today_timed;
  return tomorrow ? tomorrow_all_day : today_all_day;
}

}  // namespace

std::string eventText(const calendar::Occurrences& occurrences,
                      const calendar::DisplayConfig& display, int64_t now_s, bool tomorrow,
                      ui::Language language, std::string* shorter, std::string* longer,
                      std::string* shortest) {
  if (shorter != nullptr) shorter->clear();
  if (longer != nullptr) longer->clear();
  if (shortest != nullptr) shortest->clear();
  calendar::DisplayConfig d = display;
  d.days = 2;
  d.past = false;
  const std::vector<calendar::AgendaDay> agenda =
      calendar::buildAgenda(occurrences, now_s, language, d);
  const size_t index = tomorrow ? 1 : 0;
  if (agenda.size() <= index) return "";
  const calendar::Entry* timed = nullptr;
  const calendar::Entry* all_day = nullptr;
  for (const calendar::Entry& e : agenda[index].entries) {
    if (e.kind == calendar::Entry::Kind::kTimed && timed == nullptr && (tomorrow || e.next)) {
      timed = &e;
    }
    if (e.kind == calendar::Entry::Kind::kAllDay && !e.continues && all_day == nullptr) {
      all_day = &e;
    }
  }
  const calendar::Entry* e = timed != nullptr ? timed : all_day;
  // The next timed event of the same day after it (for the longer form).
  const calendar::Entry* second = nullptr;
  if (e != nullptr && e == timed) {
    bool after = false;
    for (const calendar::Entry& x : agenda[index].entries) {
      if (&x == e) {
        after = true;
      } else if (after && x.kind == calendar::Entry::Kind::kTimed && !x.past && !x.title.empty()) {
        second = &x;
        break;
      }
    }
  }
  if (e == nullptr || e->title.empty()) return "";
  const bool with_time = e->kind == calendar::Entry::Kind::kTimed;
  const std::vector<Text>& list = eventWordings(with_time, tomorrow);
  core::LocalTime t;
  core::toLocalTime(now_s, t);
  const Greeting greeting = tomorrow
                                ? Greeting::kEvening
                                : (t.hour < kMorningEndHour ? Greeting::kMorning : Greeting::kDay);
  const char* wording = pick(list[chooseIndex(list.size(), now_s, greeting)], language);
  const auto fillIn = [&](const char* pattern, const std::string& title) {
    std::string out = pattern;
    size_t pos;
    if ((pos = out.find("{time}")) != std::string::npos) out.replace(pos, 6, e->time);
    if ((pos = out.find("{title}")) != std::string::npos) out.replace(pos, 7, title);
    return out;
  };
  const auto finish = [](std::string out) {
    if (!endsSentence(out)) out += ".";
    return out;
  };
  const std::string text = finish(fillIn(wording, e->title));
  const bool de = language == ui::Language::kGerman;
  if (shorter != nullptr || shortest != nullptr) {
    // "09:30 Zahnarzt.", "Morgen 08:00 Zahnarzt.", "Heute: Geburtstag Oma."
    const std::string cut =
        ui::fitTextEllipsis(ui::fonts::kFraunces38, e->title, kEventTitleShortWidth);
    const char* pattern =
        with_time ? (tomorrow ? (de ? "Morgen {time} {title}" : "Tomorrow {time} {title}")
                              : "{time} {title}")
                  : (tomorrow ? (de ? "Morgen: {title}" : "Tomorrow: {title}")
                              : (de ? "Heute: {title}" : "Today: {title}"));
    const std::string s = finish(fillIn(pattern, e->title));
    if (shorter != nullptr && s != text) *shorter = s;
    if (shortest != nullptr && cut != e->title) *shortest = finish(fillIn(pattern, cut));
  }
  if (longer != nullptr && second != nullptr && !endsSentence(e->title)) {
    // "Um 09:30 Zahnarzt, danach um 14:00 Elternabend."
    std::string more = de ? ", danach um\u00a0" + second->time + " " + second->title
                          : ", then " + second->title + " at\u00a0" + second->time;
    *longer = finish(fillIn(wording, e->title) + more);
  }
  return text;
}

std::vector<Part> sentenceParts(const SentenceInput& in) {
  std::vector<Part> parts;
  if (!core::isValidUnixTime(in.now_s)) return parts;
  if (!in.online) {
    const bool de = in.language == ui::Language::kGerman;
    const std::string shorter =
        in.as_of.empty() ? "" : (de ? "Kein WLAN – Stand " : "No Wi-Fi – as of ") + in.as_of + ".";
    parts.push_back({Topic::kOffline, offlineText(in.as_of, in.language), shorter});
  } else if (!in.failed.empty()) {
    // Shorter: without the times.
    std::vector<FailedSource> plain = in.failed;
    for (FailedSource& f : plain) f.as_of.clear();
    const std::string text = failureText(in.failed, in.language);
    const std::string shorter = failureText(plain, in.language);
    parts.push_back({Topic::kFailure, text, shorter == text ? "" : shorter});
  }
  if (in.departures != nullptr) {
    std::string shorter, longer;
    const std::string text =
        cancellationText(*in.departures, in.now_s, in.language, &shorter, &longer);
    if (!text.empty()) parts.push_back({Topic::kCancellation, text, shorter, longer});
  }
  if (in.forecast != nullptr) {
    std::string shorter, longer;
    const std::string text =
        weatherSentence(*in.forecast, in.now_s, in.language, &shorter, &longer);
    if (!text.empty()) parts.push_back({Topic::kWeather, text, shorter, longer});
  }
  if (in.events != nullptr) {
    // Tomorrow from the same hour as the weather sentence.
    const weather::Forecast none;
    const bool tomorrow =
        weatherPeriod(in.forecast != nullptr ? *in.forecast : none, in.now_s) == Greeting::kEvening;
    std::string shorter, longer, shortest;
    const std::string text = eventText(*in.events, in.event_display, in.now_s, tomorrow,
                                       in.language, &shorter, &longer, &shortest);
    if (!text.empty()) parts.push_back({Topic::kEvent, text, shorter, longer, shortest});
  }
  return parts;
}

std::vector<std::string> partForms(const Part& part) {
  std::vector<std::string> forms;
  if (!part.longer.empty() && part.longer != part.text) forms.push_back(part.longer);
  forms.push_back(part.text);
  if (!part.shorter.empty() && part.shorter != part.text) forms.push_back(part.shorter);
  if (!part.shortest.empty() && part.shortest != forms.back()) forms.push_back(part.shortest);
  return forms;
}

std::string greetingAlone(Greeting greeting, int weekday, ui::Language language) {
  const std::string day = ui::weekdayName(language, weekday);
  if (day.empty()) return greetingText(greeting, language);
  if (language == ui::Language::kGerman) {
    switch (greeting) {
      case Greeting::kMorning:
        return "Guten Morgen und einen schönen " + day + ".";
      case Greeting::kDay:
        return "Guten Tag und noch einen schönen " + day + ".";
      case Greeting::kEvening:
        return "Guten Abend und einen schönen " + day + "abend.";
    }
  }
  switch (greeting) {
    case Greeting::kMorning:
      return "Good morning, have a nice " + day + ".";
    case Greeting::kDay:
      return "Good afternoon, enjoy the rest of your " + day + ".";
    case Greeting::kEvening:
      return "Good evening, have a nice " + day + " evening.";
  }
  return "";
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
