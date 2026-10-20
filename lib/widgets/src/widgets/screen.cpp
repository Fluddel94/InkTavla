#include "widgets/screen.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include "core/time.h"
#include "ui/elements.h"

namespace widgets {

namespace {

// "Stand 14:32": time of the last successful update.
std::string ageNote(ui::Language language, int64_t wall_s) {
  char time[8] = "--:--";
  core::LocalTime local;
  if (core::isValidUnixTime(wall_s) && core::toLocalTime(wall_s, local)) {
    std::snprintf(time, sizeof(time), "%02d:%02d", local.hour, local.minute);
  }
  return std::string(ui::text(language, ui::Text::kAsOf)) + " " + time;
}

hal::Rect toHal(const ui::Rect& r) { return {r.x, r.y, r.w, r.h}; }

}  // namespace

Screen::Screen(const ui::Layout& layout, ui::Language language)
    : layout_(layout), language_(language), bitmap_(ui::kScreenWidth, ui::kScreenHeight) {}

std::vector<hal::Rect> Screen::areas() const {
  std::vector<hal::Rect> result{toHal(layout_.header)};
  for (const ui::Rect& r : layout_.slots) result.push_back(toHal(r));
  return result;
}

core::FrameView Screen::render(const core::ScreenState& state) {
  bitmap_.clear(ui::Color::kWhite);
  // widgets_[0] is the header, widgets_[i] the widget of slot i - 1.
  const size_t count = std::min({widgets_.size(), state.contents.size(), layout_.slots.size() + 1});
  for (size_t i = 0; i < count; i++) {
    if (widgets_[i] == nullptr) continue;
    const ui::Rect& area = i == 0 ? layout_.header : layout_.slots[i - 1];
    drawSlot(*widgets_[i], area, state.contents[i], state);
  }
  ui::BitmapCanvas full(bitmap_);
  ui::drawSeparators(full, layout_);
  return {bitmap_.data(), bitmap_.width(), bitmap_.height(), bitmap_.stride()};
}

void Screen::drawSlot(Widget& widget, const ui::Rect& area, const core::ContentStatus& status,
                      const core::ScreenState& state) {
  ui::BitmapCanvas canvas(bitmap_, area.x, area.y, area.w, area.h);
  DrawContext context;
  context.now_s = state.wall_s;
  context.language = language_;
  context.network_connected = state.network_connected;

  switch (status.state) {
    case core::DataState::kFresh:
      widget.draw(canvas, context);
      return;
    case core::DataState::kStale:
      context.note = ageNote(language_, status.last_success_wall_s);
      context.note_alert = true;
      widget.draw(canvas, context);
      return;
    case core::DataState::kWaiting:
      ui::drawTileTitle(canvas, widget.title(language_));
      return;
    case core::DataState::kNoData:
      ui::drawTileTitle(canvas, widget.title(language_));
      if (state.network_connected) {
        ui::drawTileMessage(canvas, ui::text(language_, ui::Text::kNoData));
      } else {
        ui::drawTileMessage(canvas, ui::text(language_, ui::Text::kNoConnection),
                            ui::text(language_, ui::Text::kNoConnectionDetail));
      }
      return;
    case core::DataState::kNotSetUp: {
      const SetupMessage m = widget.setupMessage(language_);
      ui::drawTileTitle(canvas, widget.title(language_));
      ui::drawTileMessage(canvas, m.message, m.detail);
      return;
    }
  }
}

}  // namespace widgets
