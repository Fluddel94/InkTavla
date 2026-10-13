#pragma once

#include <cstdint>

namespace ui {

// Drawing surface handed to a widget for its slot.
//
// Coordinates start at (0, 0) in the top-left corner of the slot, and
// everything outside the slot is clipped, so a widget can never paint over
// another one. Drawing primitives follow with the UI design.
class Canvas {
 public:
  virtual ~Canvas() = default;

  virtual uint16_t width() const = 0;
  virtual uint16_t height() const = 0;
};

}  // namespace ui
