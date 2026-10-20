#include "core/wifi_policy.h"

#include <algorithm>

namespace core {

void WifiPolicy::attemptStarted(int64_t now_ms) {
  disconnect_seen_ = false;
  attempt_active_ = true;
  attempt_start_ms_ = now_ms;
}

void WifiPolicy::disconnected(int64_t now_ms) {
  disconnect_seen_ = true;
  wait_from_ms_ = now_ms;
}

void WifiPolicy::connected() {
  failures_ = 0;
  attempt_active_ = false;
}

WifiPolicy::Action WifiPolicy::poll(int64_t now_ms, bool is_connected, bool has_credentials) {
  if (is_connected) {
    attempt_active_ = false;
    return Action::kNone;
  }
  if (attempt_active_) {
    const bool timed_out = now_ms - attempt_start_ms_ >= kAttemptTimeoutMs;
    if (!disconnect_seen_ && !timed_out) return Action::kNone;  // still running
    attempt_active_ = false;
    failures_++;
    if (timed_out && !disconnect_seen_) wait_from_ms_ = now_ms;
  }
  constexpr int kDelays = sizeof(kRetryDelaysMs) / sizeof(kRetryDelaysMs[0]);
  const int index = std::min(failures_, kDelays - 1);
  if (now_ms - wait_from_ms_ < kRetryDelaysMs[index]) return Action::kNone;
  if (!has_credentials) return Action::kNone;

  const bool restart = failures_ > 0 && failures_ % kRestartAfter == 0;
  attemptStarted(now_ms);
  return restart ? Action::kRestartAndConnect : Action::kConnect;
}

}  // namespace core
