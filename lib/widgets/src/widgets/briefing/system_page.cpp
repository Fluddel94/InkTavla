#include "widgets/briefing/system_page.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "core/time.h"
#include "ui/elements.h"
#include "ui/fonts.h"
#include "ui/layout.h"
#include "ui/text.h"

namespace widgets::briefing {

namespace {

using ui::Color;
using ui::Language;
namespace fonts = ui::fonts;

const char* tr(Language language, const char* de, const char* en) {
  return language == Language::kGerman ? de : en;
}

// "0,9" / "0.9": one decimal with the separator of the language.
std::string decimal(Language language, uint64_t tenths) {
  return std::to_string(tenths / 10) + (language == Language::kGerman ? "," : ".") +
         std::to_string(tenths % 10);
}

std::string kilobytes(uint32_t bytes) { return std::to_string((bytes + 512) / 1024) + " KB"; }

std::string megabytes(Language language, uint32_t bytes) {
  return decimal(language, (static_cast<uint64_t>(bytes) * 10 + 524288) / 1048576) + " MB";
}

std::string seconds(Language language, uint32_t ms) {
  return decimal(language, (ms + 50) / 100) + " s";
}

// The reasons of core::restartReason() in German.
std::string reasonText(Language language, const std::string& reason) {
  if (language != Language::kGerman) return reason;
  static const struct {
    const char* en;
    const char* de;
  } kReasons[] = {
      {"power on", "Strom eingeschaltet"},
      {"reset pin", "Reset-Taste"},
      {"software restart", "Neustart durch die Firmware"},
      {"crash (panic)", "Absturz (Panic)"},
      {"watchdog (interrupt)", "Watchdog (Interrupt)"},
      {"watchdog (task)", "Watchdog (Task)"},
      {"watchdog", "Watchdog"},
      {"voltage drop (brownout)", "Spannungseinbruch"},
      {"USB (flashing or reset)", "USB (Aufspielen/Reset)"},
      {"power glitch", "Stromstörung"},
  };
  for (const auto& r : kReasons) {
    if (reason == r.en) return r.de;
  }
  return reason;
}

std::string sourceTitle(Language language, const std::string& name) {
  if (name == "mvg") return ui::text(language, ui::Text::kTitleDepartures);
  if (name == "weather") return ui::text(language, ui::Text::kTitleWeather);
  if (name == "calendar") return ui::text(language, ui::Text::kTitleCalendar);
  return name;
}

const char* stateText(Language language, core::DataState state) {
  switch (state) {
    case core::DataState::kFresh:
      return tr(language, "aktuell", "up to date");
    case core::DataState::kStale:
      return tr(language, "veraltet", "outdated");
    case core::DataState::kWaiting:
      return tr(language, "wartet", "waiting");
    case core::DataState::kNoData:
      return tr(language, "keine Daten", "no data");
    case core::DataState::kNotSetUp:
      return tr(language, "nicht eingerichtet", "not set up");
  }
  return "";
}

bool sameDay(int64_t a_s, int64_t b_s) {
  core::LocalTime a, b;
  return core::isValidUnixTime(a_s) && core::isValidUnixTime(b_s) && core::toLocalTime(a_s, a) &&
         core::toLocalTime(b_s, b) && a.year == b.year && a.month == b.month && a.day == b.day;
}

// "07:41" today, "06.11. 07:41" on another day, "–" if unknown.
std::string clockText(int64_t wall_s, int64_t now_s, bool with_seconds = false) {
  core::LocalTime t;
  if (wall_s <= 0 || !core::toLocalTime(wall_s, t)) return "–";
  char buffer[24];
  if (sameDay(wall_s, now_s)) {
    if (with_seconds) {
      std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", t.hour, t.minute, t.second);
    } else {
      std::snprintf(buffer, sizeof(buffer), "%02d:%02d", t.hour, t.minute);
    }
  } else {
    std::snprintf(buffer, sizeof(buffer), "%02d.%02d. %02d:%02d", t.day, t.month, t.hour, t.minute);
  }
  return buffer;
}

std::string logTime(const core::LogEntry& e, int64_t now_s) {
  if (e.wall_s == 0) return "+" + core::formatUptime(e.uptime_s);
  return clockText(e.wall_s, now_s, true);
}

// --- Parts ---------------------------------------------------------------------------

const ui::Font& kLabelFont = fonts::kRobotoCondensedMedium16;
const ui::Font& kValueFont = fonts::kRobotoCondensedBold16;
const ui::Font& kLogFont = fonts::kRobotoCondensedMedium15;
const ui::Font& kLogAreaFont = fonts::kRobotoCondensedBold15;
constexpr int kLogLineHeight = 18;
constexpr int kTitleBaseline = 36;

// Label and value on one baseline; the value starts at x + label_width.
void row(ui::Canvas& canvas, int x, int x1, int baseline, int label_width, const char* label,
         const std::string& value, bool warning = false) {
  ui::drawText(canvas, kLabelFont, x, baseline, label);
  int vx = x + label_width;
  if (warning) vx += ui::drawWarningSign(canvas, vx, baseline, kValueFont.cap_height) + 5;
  ui::drawText(canvas, kValueFont, vx, baseline, ui::fitTextEllipsis(kValueFont, value, x1 - vx));
}

struct Overview {
  std::string version, uptime, restart, wifi, clock, memory, psram, log;
  bool restart_warning = false, wifi_warning = false, log_warning = false;
};

Overview overviewOf(Language lang, const core::SystemReport& report) {
  const core::DeviceInfo& d = report.device;
  Overview o;
  o.version = d.firmware;
  o.uptime = core::formatUptime(d.uptime_s);
  o.restart = reasonText(lang, d.restart_reason);
  o.restart_warning = d.restart_unplanned;
  if (d.wifi_connected) {
    const std::string quality = core::signalQuality(d.rssi_dbm);
    const char* q = quality == "excellent" ? tr(lang, "sehr gut", "excellent")
                    : quality == "good"    ? tr(lang, "gut", "good")
                    : quality == "fair"    ? tr(lang, "mäßig", "fair")
                                           : tr(lang, "schwach", "weak");
    o.wifi = std::string(tr(lang, "verbunden", "connected")) + " · " + std::to_string(d.rssi_dbm) +
             " dBm (" + q + ")";
    o.wifi_warning = quality == "weak";
  } else {
    o.wifi = tr(lang, "nicht verbunden", "not connected");
    o.wifi_warning = true;
  }
  o.clock = d.time_valid ? tr(lang, "gestellt (NTP)", "set (NTP)")
                         : tr(lang, "noch nicht gestellt", "not set yet");
  o.memory = kilobytes(d.heap_free) + tr(lang, " frei · min. ", " free · min. ") +
             kilobytes(d.heap_min_free) + tr(lang, " · Block ", " · block ") +
             kilobytes(d.heap_largest);
  o.psram = megabytes(lang, d.psram_free) + tr(lang, " von ", " of ") +
            megabytes(lang, d.psram_size) + tr(lang, " frei", " free");
  o.log = std::to_string(report.warnings) +
          (report.warnings == 1 ? tr(lang, " Warnung", " warning")
                                : tr(lang, " Warnungen", " warnings")) +
          " · " + std::to_string(report.errors) +
          (report.errors == 1 ? tr(lang, " Fehler", " error") : tr(lang, " Fehler", " errors"));
  o.log_warning = report.errors > 0;
  return o;
}

// "aktuell · 07:41 (0,9 s)" and, after a failure, "Fehler 07:30: HTTP 404".
std::string sourceState(Language lang, const core::SourceReport& s, int64_t now_s) {
  std::string text = stateText(lang, s.state);
  if (s.last_success_wall_s > 0) {
    text += " · " + clockText(s.last_success_wall_s, now_s) + " (" +
            seconds(lang, s.last_duration_ms) + ")";
  }
  return text;
}

std::string sourceFailure(Language lang, const core::SourceReport& s, int64_t now_s) {
  if (s.failures == 0) return "";
  std::string text = s.failures_in_row > 0 ? tr(lang, "Fehler ", "failing ")
                                           : tr(lang, "letzter Fehler ", "last failure ");
  text += clockText(s.last_failure_wall_s, now_s) + ": " + s.problem;
  if (s.failures_in_row > 1) {
    text += " (" + std::to_string(s.failures_in_row) + "×)";
  }
  return text;
}

bool sourceWarning(const core::SourceReport& s) {
  return s.state == core::DataState::kStale || s.state == core::DataState::kNoData ||
         s.failures_in_row > 0;
}

// Marker of the level: nothing, a warning sign, or a white sign in a black
// square (error). Returns the width taken.
void levelMarker(ui::Canvas& canvas, int x, int baseline, core::LogLevel level) {
  const int h = kLogFont.cap_height;
  if (level == core::LogLevel::kWarning) {
    ui::drawWarningSign(canvas, x, baseline, h);
  } else if (level == core::LogLevel::kError) {
    const int side = h + 4;
    canvas.fillRect(x - 1, baseline - h - 2, side, side, Color::kBlack);
    ui::drawWarningSign(canvas, x + 1, baseline, h - 2, Color::kWhite);
  }
}

// The log lines from the baseline `top` down to `bottom`, newest first.
// A dotted line separates entries of an earlier start.
void drawLog(ui::Canvas& canvas, int x, int x1, int top, int bottom,
             const std::vector<core::LogEntry>& log, int64_t now_s, Language lang) {
  // Columns: time, level, area, text.
  const int time_w = ui::textWidth(kLogFont, "06.11. 07:41") + 8;
  const int level_w = kLogFont.cap_height + 10;
  const int area_w = ui::textWidth(kLogAreaFont, "calendar") + 8;
  int baseline = top;
  if (log.empty()) {
    ui::drawText(canvas, kLogFont, x, baseline,
                 tr(lang, "Noch keine Einträge.", "No entries yet."));
    return;
  }
  for (size_t i = 0; i < log.size() && baseline <= bottom; i++) {
    const core::LogEntry& e = log[i];
    if (i > 0 && log[i - 1].boot != e.boot) {
      canvas.dottedHLine(x, baseline - kLogLineHeight + 4, x1 - x, 3);
    }
    ui::drawText(canvas, kLogFont, x, baseline, logTime(e, now_s));
    levelMarker(canvas, x + time_w, baseline, e.level);
    ui::drawText(canvas, kLogAreaFont, x + time_w + level_w, baseline,
                 ui::fitText(kLogAreaFont, e.area, area_w - 4));
    const int tx = x + time_w + level_w + area_w;
    ui::drawText(canvas, kLogFont, tx, baseline, ui::fitTextEllipsis(kLogFont, e.text, x1 - tx));
    baseline += kLogLineHeight;
  }
}

// --- Page: overview and sources on the left, log on the right -------------------------

constexpr int kLeftWidth = 270;  // width of the left column
constexpr int kColumnGap = 26;
constexpr int kLogTop = kTitleBaseline + 26;  // baseline of the newest line

void drawPage(ui::Canvas& canvas, Language lang, const core::SystemReport& report,
              const std::vector<core::LogEntry>& log, int64_t now_s) {
  const int x0 = ui::kMargin;
  const int left_end = x0 + kLeftWidth;
  const int lx = left_end + kColumnGap;
  const int x1 = ui::kScreenWidth - ui::kMargin;
  ui::drawColumnTitle(canvas, x0, left_end, kTitleBaseline, tr(lang, "System", "System"),
                      "InkTavla " + report.device.firmware);
  const Overview o = overviewOf(lang, report);
  constexpr int kLabel = 78;
  constexpr int kRow = 21;
  int y = kTitleBaseline + 28;
  row(canvas, x0, left_end, y, kLabel, tr(lang, "Laufzeit", "Uptime"), o.uptime);
  y += kRow;
  row(canvas, x0, left_end, y, kLabel, tr(lang, "Neustart", "Restart"), o.restart,
      o.restart_warning);
  y += kRow;
  row(canvas, x0, left_end, y, kLabel, tr(lang, "WLAN", "Wi-Fi"), o.wifi, o.wifi_warning);
  y += kRow;
  row(canvas, x0, left_end, y, kLabel, tr(lang, "Uhrzeit", "Clock"), o.clock,
      !report.device.time_valid);
  y += kRow;
  row(canvas, x0, left_end, y, kLabel, tr(lang, "Speicher", "Memory"),
      kilobytes(report.device.heap_free) + tr(lang, " frei, min. ", " free, min. ") +
          kilobytes(report.device.heap_min_free));
  y += kRow;
  row(canvas, x0, left_end, y, kLabel, "",
      tr(lang, "größter Block ", "largest block ") + kilobytes(report.device.heap_largest));
  y += kRow;
  row(canvas, x0, left_end, y, kLabel, "PSRAM", o.psram);
  y += kRow;
  row(canvas, x0, left_end, y, kLabel, "Log", o.log, o.log_warning);

  y += 38;
  ui::drawColumnTitle(canvas, x0, left_end, y, tr(lang, "Datenquellen", "Data sources"));
  y += 24;
  for (const core::SourceReport& s : report.sources) {
    ui::drawText(canvas, kValueFont, x0, y, sourceTitle(lang, s.name));
    int sx = x0 + kLabel;
    if (sourceWarning(s)) sx += ui::drawWarningSign(canvas, sx, y, kValueFont.cap_height) + 5;
    ui::drawText(canvas, kLabelFont, sx, y,
                 ui::fitTextEllipsis(kLabelFont, sourceState(lang, s, now_s), left_end - sx));
    const std::string failure = sourceFailure(lang, s, now_s);
    if (!failure.empty()) {
      y += 19;
      ui::drawText(
          canvas, fonts::kRobotoCondensedMedium15, x0 + kLabel, y,
          ui::fitTextEllipsis(fonts::kRobotoCondensedMedium15, failure, left_end - x0 - kLabel));
    }
    y += 25;
  }
  ui::drawText(canvas, kLabelFont, x0, ui::kScreenHeight - ui::kMarginBottom,
               tr(lang, "KEY2: zurück", "KEY2: back"));

  canvas.dottedVLine(left_end + kColumnGap / 2, kTitleBaseline - 12,
                     ui::kScreenHeight - ui::kMarginBottom - kTitleBaseline + 12, 2);
  ui::drawColumnTitle(canvas, lx, x1, kTitleBaseline, tr(lang, "Protokoll", "Log"),
                      tr(lang, "neueste zuerst", "newest first"));
  drawLog(canvas, lx, x1, kLogTop, ui::kScreenHeight - ui::kMarginBottom, log, now_s, lang);
}

}  // namespace

size_t systemPageLogLines() {
  const int bottom = ui::kScreenHeight - ui::kMarginBottom;
  return static_cast<size_t>((bottom - kLogTop) / kLogLineHeight + 1);
}

void drawSystemPage(ui::Canvas& canvas, ui::Language language, const core::SystemReport& report,
                    const std::vector<core::LogEntry>& log, int64_t now_s) {
  drawPage(canvas, language, report, log, now_s);
}

}  // namespace widgets::briefing
