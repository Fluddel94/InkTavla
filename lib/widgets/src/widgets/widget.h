#pragma once

// The two parts a widget is made of: data sources and views.
//
// - A data source fetches and keeps data, e.g. the departures of a station,
//   a forecast or a calendar. The display controller sees it as
//   core::Content: it decides when the source fetches and tracks how fresh
//   its data is. One source can be read by several views.
// - A view draws one area of the screen (the header or a slot of the
//   layout) from the data of one or more sources. A view that reads several
//   sources (e.g. a summary of weather, calendar and departures) gets the
//   state of each of them.
//
// Where a view is drawn is decided by the screen (widgets/screen.h), when by
// the display controller; a view can never paint outside its area. Which
// widgets exist is listed in widgets/registry.h.

#include <cstdint>
#include <string>
#include <vector>

#include "core/display_controller.h"
#include "core/freshness.h"
#include "ui/canvas.h"
#include "ui/i18n.h"

namespace widgets {

// Message shown instead of a view when its source lacks settings.
struct SetupMessage {
  std::string message;  // e.g. "Haltestelle fehlt"
  std::string detail;   // e.g. "config.json: widgets.mvg.station"
};

class DataSource : public core::Content {
 public:
  // Stable identifier used in the configuration, e.g. "mvg".
  virtual const char* id() const = 0;
  // Message when isSetUp() is false.
  virtual SetupMessage setupMessage(ui::Language language) const;
};

// State of a data source when a frame is drawn.
struct SourceStatus {
  core::DataState state = core::DataState::kFresh;
  int64_t last_success_wall_s = 0;  // Unix seconds, 0 if none
};

struct DrawContext {
  int64_t now_s = 0;  // current Unix time (invalid before NTP)
  ui::Language language = ui::Language::kGerman;
  // Network as shown: offline only after a grace period (see
  // core::DisplayController::kOfflineGraceMs).
  bool network_connected = false;
  // Right part of the tile title; with note_alert shown as a badge (age
  // hint "Stand 14:32" for old data of the view's main source).
  std::string note;
  bool note_alert = false;
  // State of every source of the view, in the order of View::sources().
  std::vector<SourceStatus> sources;
};

class View {
 public:
  virtual ~View() = default;
  // Data sources the view reads. The first one is its main source: while it
  // has no data or lacks settings, the screen shows a message instead of
  // calling draw(), and old data gets the age badge. A view without sources
  // (e.g. the header) is always drawn.
  virtual std::vector<const DataSource*> sources() const { return {}; }
  // Tile title, also used for messages when the view cannot be drawn.
  virtual std::string title(ui::Language language) const = 0;
  // Draws the current data into the area.
  virtual void draw(ui::Canvas& canvas, const DrawContext& context) = 0;
};

}  // namespace widgets
