#pragma once

// The screen: composes the frame from the header and the widgets of the
// layout's slots (the "where" of the display controller, see
// core/display_controller.h).
//
// Per slot it decides between the widget's own drawing and a message, from
// the freshness of the widget's data:
//  - fresh: the widget draws itself
//  - stale (updates failing for 60 s or more): the widget draws its last data
//    with the badge "Stand 14:32" in its title
//  - waiting for the first data: only the tile title
//  - no data for 60 s or more: "Keine Verbindung" (no Wi-Fi) or "Keine Daten"
//  - not set up: the widget's setup message

#include <utility>
#include <vector>

#include "core/display_controller.h"
#include "ui/bitmap.h"
#include "ui/i18n.h"
#include "ui/layout.h"
#include "widgets/widget.h"

namespace widgets {

class Screen : public core::ScreenRenderer {
 public:
  Screen(const ui::Layout& layout, ui::Language language);

  // Widgets in the order of the controller's contents: the header first,
  // then one per slot of the layout. They must outlive the screen.
  void setWidgets(std::vector<Widget*> widgets) { widgets_ = std::move(widgets); }

  core::FrameView render(const core::ScreenState& state) override;
  std::vector<hal::Rect> areas() const override;

  const ui::Bitmap& bitmap() const { return bitmap_; }

 private:
  void drawSlot(Widget& widget, const ui::Rect& area, const core::ContentStatus& status,
                const core::ScreenState& state);

  ui::Layout layout_;
  ui::Language language_;
  ui::Bitmap bitmap_;
  std::vector<Widget*> widgets_;
};

}  // namespace widgets
