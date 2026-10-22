#pragma once

// Base of the views of the "Briefing" columns: titles in spaced capitals
// without a frame or rule, messages below the title (ui::drawColumnTitle,
// ui::drawColumnMessage).

#include <string_view>

#include "ui/elements.h"
#include "widgets/widget.h"

namespace widgets {

class ColumnView : public View {
 public:
  void drawTitle(ui::Canvas& canvas, ui::Language language) override {
    ui::drawColumnTitle(canvas, 0, canvas.width(), ui::kColumnTitleBaseline, title(language));
  }
  void drawMessage(ui::Canvas& canvas, ui::Language language, std::string_view message,
                   std::string_view detail) override {
    drawTitle(canvas, language);
    ui::drawColumnMessage(canvas, message, detail);
  }
};

}  // namespace widgets
