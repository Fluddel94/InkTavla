#include "widgets/screen.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include "core/time.h"
#include "ui/elements.h"
#include "widgets/briefing/access.h"

namespace widgets {

// "Stand 14:32": time of the last successful update.
std::string ageNote(ui::Language language, int64_t wall_s) {
  char time[8] = "--:--";
  core::LocalTime local;
  if (core::isValidUnixTime(wall_s) && core::toLocalTime(wall_s, local)) {
    std::snprintf(time, sizeof(time), "%02d:%02d", local.hour, local.minute);
  }
  return std::string(ui::text(language, ui::Text::kAsOf)) + " " + time;
}

namespace {

hal::Rect toHal(const ui::Rect& r) { return {r.x, r.y, r.w, r.h}; }

}  // namespace

Screen::Screen(const ui::Layout& layout, ui::Language language)
    : layout_(layout), language_(language), bitmap_(ui::kScreenWidth, ui::kScreenHeight) {}

std::vector<hal::Rect> Screen::areas() const {
  std::vector<hal::Rect> result;
  for (const ui::Rect& r : layout_.header_areas) result.push_back(toHal(r));
  for (const ui::Rect& r : layout_.slots) result.push_back(toHal(r));
  return result;
}

core::FrameView Screen::render(const core::ScreenState& state) {
  bitmap_.clear(ui::Color::kWhite);
  DrawContext base;
  base.now_s = state.wall_s;
  base.language = language_;
  base.network_connected = state.network_connected;
  if (state.system_page) {
    ui::BitmapCanvas canvas(bitmap_, 0, 0, ui::kScreenWidth, ui::kScreenHeight);
    const core::SystemReport report = system_report_ ? system_report_() : core::SystemReport{};
    const auto log = core::systemLog().newest(briefing::systemPageLogLines());
    briefing::drawSystemPage(canvas, language_, report, log, state.wall_s);
    return {bitmap_.data(), bitmap_.width(), bitmap_.height(), bitmap_.stride()};
  }
  if (state.starting) {
    ui::BitmapCanvas canvas(bitmap_, 0, 0, ui::kScreenWidth, ui::kScreenHeight);
    briefing::drawStartScreen(canvas, language_, start_info_, !state.network_up);
    return {bitmap_.data(), bitmap_.width(), bitmap_.height(), bitmap_.stride()};
  }
  if (state.notice.mode == core::AccessMode::kWifiNotSetUp ||
      state.notice.mode == core::AccessMode::kWifiNotConnected) {
    // No Wi-Fi: the whole screen shows how to set it up.
    ui::BitmapCanvas canvas(bitmap_, 0, 0, ui::kScreenWidth, ui::kScreenHeight);
    briefing::drawWifiScreen(canvas, base, state.notice);
    return {bitmap_.data(), bitmap_.width(), bitmap_.height(), bitmap_.stride()};
  }
  // views_[0] is the band, views_[i] the view of column i - 1.
  const size_t count = std::min(views_.size(), layout_.slots.size() + 1);
  for (size_t i = 0; i < count; i++) {
    if (views_[i] == nullptr) continue;
    const ui::Rect area = i == 0 ? layout_.header : ui::contentOf(layout_, i - 1);
    if (i == 0 && state.notice.mode == core::AccessMode::kSettings) {
      // Settings page open: its notice instead of the sentence.
      ui::BitmapCanvas canvas(bitmap_, area.x, area.y, area.w, area.h);
      briefing::drawSettingsBand(canvas, base, state.notice);
      continue;
    }
    drawArea(*views_[i], area, state);
  }
  for (const ui::Rect& d : layout_.dividers) {
    ui::BitmapCanvas canvas(bitmap_, 0, 0, ui::kScreenWidth, ui::kScreenHeight);
    canvas.dottedVLine(d.x, d.y, d.h, 2);
  }
  return {bitmap_.data(), bitmap_.width(), bitmap_.height(), bitmap_.stride()};
}

SourceStatus Screen::statusOf(const DataSource* source, const core::ScreenState& state) const {
  for (size_t i = 0; i < sources_.size() && i < state.contents.size(); i++) {
    if (sources_[i] == source) {
      return {state.contents[i].state, state.contents[i].last_success_wall_s};
    }
  }
  return {core::DataState::kWaiting, 0};  // not known to the controller
}

void Screen::drawArea(View& view, const ui::Rect& area, const core::ScreenState& state) {
  ui::BitmapCanvas canvas(bitmap_, area.x, area.y, area.w, area.h);
  DrawContext context;
  context.now_s = state.wall_s;
  context.language = language_;
  context.network_connected = state.network_connected;
  const std::vector<const DataSource*> sources = view.sources();
  for (const DataSource* source : sources) context.sources.push_back(statusOf(source, state));
  if (sources.empty() || view.handlesStates()) {
    view.draw(canvas, context);
    return;
  }

  const SourceStatus& main = context.sources.front();
  switch (main.state) {
    case core::DataState::kFresh:
      view.draw(canvas, context);
      return;
    case core::DataState::kStale:
      context.note = ageNote(language_, main.last_success_wall_s);
      context.note_alert = true;
      view.draw(canvas, context);
      return;
    case core::DataState::kWaiting: {
      const Problem p = sources.front()->problem(language_);
      if (p.settings && !p.message.empty()) {
        view.drawMessage(canvas, language_, p.message, p.detail);
        return;
      }
      view.drawMessage(
          canvas, language_, ui::text(language_, ui::Text::kLoading),
          state.network_connected ? "" : ui::text(language_, ui::Text::kNoConnectionDetail), false);
      return;
    }
    case core::DataState::kNoData:
      if (state.network_connected) {
        const Problem p = sources.front()->problem(language_);
        if (!p.message.empty()) {
          view.drawMessage(canvas, language_, p.message, p.detail);
        } else {
          view.drawMessage(canvas, language_, ui::text(language_, ui::Text::kNoData), "");
        }
      } else {
        view.drawMessage(canvas, language_, ui::text(language_, ui::Text::kNoConnection),
                         ui::text(language_, ui::Text::kNoConnectionDetail));
      }
      return;
    case core::DataState::kNotSetUp: {
      const SetupMessage m = sources.front()->setupMessage(language_);
      view.drawMessage(canvas, language_, m.message, m.detail);
      return;
    }
  }
}

}  // namespace widgets
