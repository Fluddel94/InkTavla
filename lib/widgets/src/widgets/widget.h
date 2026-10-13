#pragma once

#include <cstdint>

#include "ui/canvas.h"

namespace widgets {

// Common interface of all widgets.
//
// A widget decides *what* is shown: a provider fetches the data, the widget
// keeps it in its data model and draws it onto the canvas of its slot.
// Supported slot sizes, data fetching and configuration fields are added
// together with the first widget.
class Widget {
 public:
  virtual ~Widget() = default;

  // Stable identifier used in the configuration, e.g. "transit".
  virtual const char* id() const = 0;

  // How often the widget needs fresh data, in seconds.
  virtual uint32_t updateIntervalSeconds() const = 0;

  // Draws the current data model into the slot.
  virtual void draw(ui::Canvas& canvas) = 0;
};

}  // namespace widgets
