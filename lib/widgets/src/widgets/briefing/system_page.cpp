#include "widgets/briefing/system_page.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "core/time.h"
#include "ui/elements.h"
#include "ui/fonts.h"
#include "ui/i18n.h"
#include "ui/layout.h"
#include "ui/text.h"

namespace widgets::briefing {

namespace {

using ui::Color;
namespace fonts = ui::fonts;

// The page is always English, whatever the language of the display: the log
// is English (fixed texts of the firmware), and so is everything around it,
// so a photo or a download can be shared to get help (Tobias 2026-11-07).

std::string decimal(uint64_t tenths) {
  return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

std::string kilobytes(uint32_t bytes) { return std::to_string((bytes + 512) / 1024) + " KB"; }

std::string megabytes(uint32_t bytes) {
  return decimal((static_cast<uint64_t>(bytes) * 10 + 524288) / 1048576) + " MB";
}

std::string seconds(uint32_t ms) { return decimal((ms + 50) / 100) + " s"; }

std::string sourceTitle(const std::string& name) {
  if (name == "mvg") return ui::text(ui::Language::kEnglish, ui::Text::kTitleDepartures);
  if (name == "weather") return ui::text(ui::Language::kEnglish, ui::Text::kTitleWeather);
  if (name == "calendar") return ui::text(ui::Language::kEnglish, ui::Text::kTitleCalendar);
  return name;
}

const char* stateText(core::DataState state) {
  switch (state) {
    case core::DataState::kFresh:
      return "up to date";
    case core::DataState::kStale:
      return "outdated";
    case core::DataState::kWaiting:
      return "waiting";
    case core::DataState::kNoData:
      return "no data";
    case core::DataState::kNotSetUp:
      return "not set up";
  }
  return "";
}

bool sameDay(int64_t a_s, int64_t b_s) {
  core::LocalTime a, b;
  return core::isValidUnixTime(a_s) && core::isValidUnixTime(b_s) && core::toLocalTime(a_s, a) &&
         core::toLocalTime(b_s, b) && a.year == b.year && a.month == b.month && a.day == b.day;
}

// "07:41" today, "11-06 07:41" (month-day, like the date of the download)
// on another day, "–" if unknown.
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
    std::snprintf(buffer, sizeof(buffer), "%02d-%02d %02d:%02d", t.month, t.day, t.hour, t.minute);
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
  std::string version, uptime, restart, wifi, signal, clock, psram, log;
  bool restart_warning = false, wifi_warning = false, log_warning = false;
};

Overview overviewOf(const core::SystemReport& report) {
  const core::DeviceInfo& d = report.device;
  Overview o;
  o.version = d.firmware;
  o.uptime = core::formatUptime(d.uptime_s);
  o.restart = d.restart_reason;
  o.restart_warning = d.restart_unplanned;
  // Two rows, so even "-100 dBm (excellent)" fits; a warning only without
  // a connection (a weak signal is said in words).
  o.wifi = d.wifi_connected ? "connected" : "not connected";
  o.wifi_warning = !d.wifi_connected;
  if (d.wifi_connected) {
    o.signal = std::to_string(d.rssi_dbm) + " dBm (" + core::signalQuality(d.rssi_dbm) + ")";
  } else {
    o.signal = "–";
  }
  o.clock = d.time_valid ? "set (NTP)" : "not set yet";
  o.psram = megabytes(d.psram_free) + " of " + megabytes(d.psram_size) + " free";
  o.log = std::to_string(report.warnings) + (report.warnings == 1 ? " warning" : " warnings") +
          " · " + std::to_string(report.errors) + (report.errors == 1 ? " error" : " errors");
  o.log_warning = report.errors > 0;
  return o;
}

// "up to date · 07:41 (0.9 s)" and, after a failure, "failing 07:30: HTTP 404".
std::string sourceState(const core::SourceReport& s, int64_t now_s) {
  std::string text = stateText(s.state);
  if (s.last_success_wall_s > 0) {
    text +=
        " · " + clockText(s.last_success_wall_s, now_s) + " (" + seconds(s.last_duration_ms) + ")";
  }
  return text;
}

std::string sourceFailure(const core::SourceReport& s, int64_t now_s) {
  if (s.failures == 0) return "";
  std::string text = s.failures_in_row > 0 ? "failing " : "last failure ";
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

// Columns of the log: time, level, area, text (gaps of 6 px, so the longest
// texts of core/log_texts.h fit).
int logTimeWidth() { return ui::textWidth(kLogFont, "11-06 07:41") + 6; }
int logLevelWidth() { return kLogFont.cap_height + 10; }
int logAreaWidth() { return ui::textWidth(kLogAreaFont, "calendar") + 6; }

// The log lines from the baseline `top` down to `bottom`, newest first.
// A dotted line separates entries of an earlier start.
void drawLog(ui::Canvas& canvas, int x, int x1, int top, int bottom,
             const std::vector<core::LogEntry>& log, int64_t now_s) {
  const int time_w = logTimeWidth();
  const int level_w = logLevelWidth();
  const int area_w = logAreaWidth();
  int baseline = top;
  if (log.empty()) {
    ui::drawText(canvas, kLogFont, x, baseline, "No entries yet.");
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
constexpr int kLabel = 78;                    // width of the labels on the left

int logLeft() { return ui::kMargin + kLeftWidth + kColumnGap; }

void drawPage(ui::Canvas& canvas, const core::SystemReport& report,
              const std::vector<core::LogEntry>& log, int64_t now_s) {
  const int x0 = ui::kMargin;
  const int left_end = x0 + kLeftWidth;
  const int lx = logLeft();
  const int x1 = ui::kScreenWidth - ui::kMargin;
  ui::drawColumnTitle(canvas, x0, left_end, kTitleBaseline, "System",
                      "InkTavla " + report.device.firmware);
  const Overview o = overviewOf(report);
  constexpr int kRow = 21;
  int y = kTitleBaseline + 28;
  row(canvas, x0, left_end, y, kLabel, "Uptime", o.uptime);
  y += kRow;
  row(canvas, x0, left_end, y, kLabel, "Restart", o.restart, o.restart_warning);
  y += kRow;
  row(canvas, x0, left_end, y, kLabel, "Wi-Fi", o.wifi, o.wifi_warning);
  y += kRow;
  row(canvas, x0, left_end, y, kLabel, "Signal", o.signal);
  y += kRow;
  row(canvas, x0, left_end, y, kLabel, "Clock", o.clock, !report.device.time_valid);
  y += kRow;
  row(canvas, x0, left_end, y, kLabel, "Memory",
      kilobytes(report.device.heap_free) + " free, min. " + kilobytes(report.device.heap_min_free));
  y += kRow;
  row(canvas, x0, left_end, y, kLabel, "",
      "largest block " + kilobytes(report.device.heap_largest));
  y += kRow;
  row(canvas, x0, left_end, y, kLabel, "PSRAM", o.psram);
  y += kRow;
  row(canvas, x0, left_end, y, kLabel, "Log", o.log, o.log_warning);

  y += 34;
  ui::drawColumnTitle(canvas, x0, left_end, y, "Data sources");
  y += 24;
  for (const core::SourceReport& s : report.sources) {
    ui::drawText(canvas, kValueFont, x0, y, sourceTitle(s.name));
    int sx = x0 + kLabel;
    if (sourceWarning(s)) sx += ui::drawWarningSign(canvas, sx, y, kValueFont.cap_height) + 5;
    ui::drawText(canvas, kLabelFont, sx, y,
                 ui::fitTextEllipsis(kLabelFont, sourceState(s, now_s), left_end - sx));
    const std::string failure = sourceFailure(s, now_s);
    if (!failure.empty()) {
      y += 19;
      ui::drawText(
          canvas, fonts::kRobotoCondensedMedium15, x0 + kLabel, y,
          ui::fitTextEllipsis(fonts::kRobotoCondensedMedium15, failure, left_end - x0 - kLabel));
    }
    y += 25;
  }
  ui::drawText(canvas, kLabelFont, x0, ui::kScreenHeight - ui::kMarginBottom, "KEY2: back");

  canvas.dottedVLine(left_end + kColumnGap / 2, kTitleBaseline - 12,
                     ui::kScreenHeight - ui::kMarginBottom - kTitleBaseline + 12, 2);
  ui::drawColumnTitle(canvas, lx, x1, kTitleBaseline, "Log", "newest first");
  drawLog(canvas, lx, x1, kLogTop, ui::kScreenHeight - ui::kMarginBottom, log, now_s);
}

}  // namespace

size_t systemPageLogLines() {
  const int bottom = ui::kScreenHeight - ui::kMarginBottom;
  return static_cast<size_t>((bottom - kLogTop) / kLogLineHeight + 1);
}

int systemPageLogTextWidth() {
  const int text_x = logLeft() + logTimeWidth() + logLevelWidth() + logAreaWidth();
  return ui::kScreenWidth - ui::kMargin - text_x;
}

int systemPageValueWidth() { return kLeftWidth - kLabel; }

void drawSystemPage(ui::Canvas& canvas, const core::SystemReport& report,
                    const std::vector<core::LogEntry>& log, int64_t now_s) {
  drawPage(canvas, report, log, now_s);
}

}  // namespace widgets::briefing
