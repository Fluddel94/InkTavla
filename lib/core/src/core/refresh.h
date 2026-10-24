#pragma once

// Decides how the panel is refreshed: not at all if nothing changed, only
// the changed areas (partial refresh), or the whole panel (full refresh)
// at the first update and regularly against ghosting.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "hal/display.h"

namespace core {

// 1-bit frame (see hal::Display): rows of `stride` bytes, MSB = left pixel.
struct FrameView {
  const uint8_t* data = nullptr;
  uint16_t width = 0;
  uint16_t height = 0;
  size_t stride = 0;
};

// True if any pixel inside the rectangle differs between a and b (both of
// the same size).
bool regionDiffers(const FrameView& a, const FrameView& b, const hal::Rect& r);

struct RefreshPlan {
  hal::Refresh mode = hal::Refresh::kPartial;
  std::vector<hal::Rect> regions;  // changed areas (kPartial); empty = nothing to do

  bool needed() const { return mode == hal::Refresh::kFull || !regions.empty(); }
};

class RefreshPlanner {
 public:
  // Full refresh at the latest every full_refresh_minutes (when something
  // changes).
  explicit RefreshPlanner(int full_refresh_minutes);

  // Plans the refresh from the frame shown so far (nullptr data = none yet)
  // to the next one, checking the given areas (header, slots).
  RefreshPlan plan(const FrameView& shown, const FrameView& next,
                   const std::vector<hal::Rect>& areas, int64_t now_ms) const;

  // The next plan is a full refresh (also if nothing changed).
  void requestFull() { full_requested_ = true; }

  // Reports that a plan was carried out.
  void done(const RefreshPlan& plan, int64_t now_ms);

 private:
  int64_t full_interval_ms_;
  bool had_full_ = false;
  bool full_requested_ = false;
  int64_t last_full_ms_ = 0;
};

}  // namespace core
