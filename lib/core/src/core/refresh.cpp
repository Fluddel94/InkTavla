#include "core/refresh.h"

#include <algorithm>

namespace core {

bool regionDiffers(const FrameView& a, const FrameView& b, const hal::Rect& r) {
  const int x0 = std::max(r.x, 0);
  const int y0 = std::max(r.y, 0);
  const int x1 = std::min(r.x + r.w, static_cast<int>(a.width));
  const int y1 = std::min(r.y + r.h, static_cast<int>(a.height));
  if (x0 >= x1 || y0 >= y1) return false;
  const int first_byte = x0 / 8;
  const int last_byte = (x1 - 1) / 8;
  const uint8_t first_mask = 0xFF >> (x0 % 8);
  const uint8_t last_mask = static_cast<uint8_t>(0xFF << (7 - (x1 - 1) % 8));
  for (int y = y0; y < y1; y++) {
    const uint8_t* ra = a.data + y * a.stride;
    const uint8_t* rb = b.data + y * b.stride;
    for (int i = first_byte; i <= last_byte; i++) {
      uint8_t mask = 0xFF;
      if (i == first_byte) mask &= first_mask;
      if (i == last_byte) mask &= last_mask;
      if ((ra[i] ^ rb[i]) & mask) return true;
    }
  }
  return false;
}

RefreshPlanner::RefreshPlanner(int full_refresh_minutes)
    : full_interval_ms_(static_cast<int64_t>(std::max(full_refresh_minutes, 1)) * 60000) {}

RefreshPlan RefreshPlanner::plan(const FrameView& shown, const FrameView& next,
                                 const std::vector<hal::Rect>& areas, int64_t now_ms) const {
  RefreshPlan p;
  if (shown.data == nullptr || !had_full_ || full_requested_) {
    p.mode = hal::Refresh::kFull;
    return p;
  }
  for (const hal::Rect& r : areas) {
    if (regionDiffers(shown, next, r)) p.regions.push_back(r);
  }
  if (!p.regions.empty() && now_ms - last_full_ms_ >= full_interval_ms_) {
    p.mode = hal::Refresh::kFull;
    p.regions.clear();
  }
  return p;
}

void RefreshPlanner::done(const RefreshPlan& plan, int64_t now_ms) {
  if (plan.mode == hal::Refresh::kFull) {
    had_full_ = true;
    full_requested_ = false;
    last_full_ms_ = now_ms;
  }
}

}  // namespace core
