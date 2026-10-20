#pragma once

// Common interface of all widgets.
//
// A widget decides *what* is shown: it fetches its data (core::Content, the
// part the display controller sees), keeps it and draws it onto the canvas
// of its slot. Where and when it is drawn is decided by the display
// controller (core) and the screen (widgets/screen.h); a widget can never
// paint outside its slot.

#include <cstdint>
#include <string>

#include "core/display_controller.h"
#include "ui/canvas.h"
#include "ui/i18n.h"

namespace widgets {

struct DrawContext {
  int64_t now_s = 0;  // current Unix time (invalid before NTP)
  ui::Language language = ui::Language::kGerman;
  bool network_connected = false;
  // Right part of the tile title; with note_alert shown as a badge (age
  // hint "Stand 14:32" for old data).
  std::string note;
  bool note_alert = false;
};

// Message shown instead of the widget when settings are missing.
struct SetupMessage {
  std::string message;  // e.g. "Haltestelle fehlt"
  std::string detail;   // e.g. "config.json: widgets.mvg.station"
};

class Widget : public core::Content {
 public:
  // Stable identifier used in the configuration, e.g. "mvg".
  virtual const char* id() const = 0;

  // Tile title, used for messages when the widget has no data.
  virtual std::string title(ui::Language language) const = 0;

  // Message when isSetUp() is false.
  virtual SetupMessage setupMessage(ui::Language language) const;

  // Draws the current data into the slot.
  virtual void draw(ui::Canvas& canvas, const DrawContext& context) = 0;
};

}  // namespace widgets
