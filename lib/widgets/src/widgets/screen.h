#pragma once

// The screen: composes the frame from the views of the band and the
// layout's columns (the "where" of the display controller, see
// core/display_controller.h).
//
// Per area it decides between the view's own drawing and a message, from
// the freshness of the data of the view's main source (the first of
// View::sources()):
//  - fresh: the view draws itself
//  - stale (updates failing for 60 s or more): the view draws the last data
//    with the badge "Stand 14:32" in its title
//  - waiting for the first data: only the column title
//  - no data for 60 s or more: "Keine Verbindung" (no Wi-Fi) or "Keine Daten"
//  - not set up: the source's setup message
// Views without sources are always drawn.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/display_controller.h"
#include "ui/bitmap.h"
#include "ui/i18n.h"
#include "ui/layout.h"
#include "widgets/widget.h"

namespace widgets {

// "Stand 14:32" / "As of 14:32": the time of a last successful update.
std::string ageNote(ui::Language language, int64_t wall_s);

class Screen : public core::ScreenRenderer {
 public:
  Screen(const ui::Layout& layout, ui::Language language);

  // The data sources in the order they were added to the display
  // controller, i.e. the order of ScreenState::contents.
  void setSources(std::vector<const DataSource*> sources) { sources_ = std::move(sources); }
  // The band view first, then one view per column of the layout. Sources
  // and views must outlive the screen.
  void setViews(std::vector<View*> views) { views_ = std::move(views); }

  core::FrameView render(const core::ScreenState& state) override;
  std::vector<hal::Rect> areas() const override;

  const ui::Bitmap& bitmap() const { return bitmap_; }

 private:
  void drawArea(View& view, const ui::Rect& area, const core::ScreenState& state);
  SourceStatus statusOf(const DataSource* source, const core::ScreenState& state) const;

  ui::Layout layout_;
  ui::Language language_;
  ui::Bitmap bitmap_;
  std::vector<const DataSource*> sources_;
  std::vector<View*> views_;
};

}  // namespace widgets
