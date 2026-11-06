#pragma once

// The system page (KEY2): the state of the device at a glance and the
// newest lines of the system log, on the whole screen. Closed with KEY2 or
// after two minutes (core::DisplayController::kSystemPageMs).

#include <cstdint>
#include <vector>

#include "core/syslog.h"
#include "core/system_report.h"
#include "ui/canvas.h"
#include "ui/i18n.h"

namespace widgets::briefing {

// Left: version, uptime, reason of the last restart, Wi-Fi signal, clock,
// memory, number of warnings and errors, and per data source its state,
// last success and last failure. Right: the newest log lines, newest first
// (fixed English texts of the log); entries of an earlier start below a
// dotted line. Chosen 2026-11-06 from two drafts.

// Number of log lines the page shows at most (the newest).
size_t systemPageLogLines();

// log: the newest entries, newest first. now_s: current Unix time (invalid
// before NTP).
void drawSystemPage(ui::Canvas& canvas, ui::Language language, const core::SystemReport& report,
                    const std::vector<core::LogEntry>& log, int64_t now_s);

}  // namespace widgets::briefing
