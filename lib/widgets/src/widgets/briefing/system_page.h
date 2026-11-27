#pragma once

// The system page (KEY2): the state of the device at a glance and the
// newest lines of the system log, on the whole screen. Closed with KEY2 or
// after two minutes (core::DisplayController::kSystemPageMs).

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/syslog.h"
#include "core/system_report.h"
#include "ui/canvas.h"

namespace widgets::briefing {

// Title: the firmware version. Left: uptime, reason of the last restart, Wi-Fi connection and
// signal with its drops ("Drops 1 · Last 24h 0"), clock, memory, number of
// warnings and errors, and per data source its state, last success, last
// failure and its fails ("Fails 3 · Last 24h 1"; a fail is a series of
// failed attempts, core/fault_counts.h). Right: the newest log lines,
// newest first (fixed English texts of the log); entries of an earlier
// start below a dotted line. Chosen 2026-11-06 from two drafts; always
// English since 2026-11-07, whatever the language of the display.

// Number of log lines the page shows at most (the newest).
size_t systemPageLogLines();

// Width in pixels for the text of a log line (longer texts end in "…"), and
// for a value in the overview on the left (for tests).
int systemPageLogTextWidth();
int systemPageValueWidth();

// Baseline of the last line on the left with this many sources, each with
// a failure line (the most the page shows; for tests).
int systemPageSourcesBottom(size_t sources);

// log: the newest entries, newest first. now_s: current Unix time (invalid
// before NTP).
void drawSystemPage(ui::Canvas& canvas, const core::SystemReport& report,
                    const std::vector<core::LogEntry>& log, int64_t now_s);

}  // namespace widgets::briefing
