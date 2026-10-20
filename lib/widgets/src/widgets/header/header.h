#pragma once

// Header: date on the left, time and Wi-Fi status on the right (overall UI
// design, style A). Drawn by the display controller into the header area of
// the layout; it needs no data of its own.

#include <cstdint>

#include "ui/canvas.h"
#include "ui/i18n.h"
#include "widgets/widget.h"

namespace widgets::header {

struct State {
  int64_t now_s = 0;  // current Unix time, 0 or invalid if not synchronised yet
  ui::Language language = ui::Language::kGerman;
  bool wifi_connected = false;
};

// Draws the header into a canvas of the header's size (800 x 40).
void draw(ui::Canvas& canvas, const State& state);

// The header as a widget: no data of its own, everything comes from the
// draw context.
class HeaderWidget : public Widget {
 public:
  const char* id() const override { return "header"; }
  std::string title(ui::Language) const override { return ""; }
  void draw(ui::Canvas& canvas, const DrawContext& context) override {
    header::draw(canvas, {context.now_s, context.language, context.network_connected});
  }
};

}  // namespace widgets::header
