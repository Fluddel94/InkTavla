#include "core/freshness.h"

#include "core/time.h"

namespace core {

void Freshness::success(int64_t now_ms, int64_t wall_s) {
  (void)now_ms;
  has_data_ = true;
  failing_ = false;
  last_success_wall_s_ = isValidUnixTime(wall_s) ? wall_s : 0;
}

void Freshness::failure(int64_t now_ms) {
  if (failing_) return;  // the disturbance counts from the first failure
  failing_ = true;
  failing_since_ms_ = now_ms;
}

DataState Freshness::state(int64_t now_ms) const {
  if (not_set_up_) return DataState::kNotSetUp;
  const bool beyond_grace = failing_ && now_ms - failing_since_ms_ >= kGraceMs;
  if (!has_data_) return beyond_grace ? DataState::kNoData : DataState::kWaiting;
  return beyond_grace ? DataState::kStale : DataState::kFresh;
}

}  // namespace core
