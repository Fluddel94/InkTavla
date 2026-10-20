#pragma once

// When to (re)connect to Wi-Fi, ported from wifi_conn of the MVG departure
// display. The rules only decide; core/wifi.h carries them out on the
// device. All times are monotonic milliseconds.
//
// - A new attempt only starts when the running one has ended (disconnect
//   reported or kAttemptTimeoutMs passed).
// - Pauses before the next attempt grow with the number of failures in a
//   row: 2, 5, 15, 30, 30, ... s.
// - After every kRestartAfter failures in a row, Wi-Fi is switched off and
//   on again before the attempt.

#include <cstdint>

namespace core {

class WifiPolicy {
 public:
  static constexpr int64_t kAttemptTimeoutMs = 10000;
  static constexpr int64_t kRetryDelaysMs[] = {2000, 5000, 15000, 30000};
  static constexpr int kRestartAfter = 6;

  enum class Action {
    kNone,
    kConnect,            // start an attempt with the stored credentials
    kRestartAndConnect,  // switch Wi-Fi off and on, then start an attempt
  };

  // An attempt was started (also from outside, e.g. Improv).
  void attemptStarted(int64_t now_ms);
  // The connection attempt failed or an existing connection was lost.
  void disconnected(int64_t now_ms);
  // Connected with an IP address.
  void connected();

  // Called regularly; says what to do now.
  Action poll(int64_t now_ms, bool is_connected, bool has_credentials);

  // Failures in a row since the last connection.
  int failures() const { return failures_; }

 private:
  bool attempt_active_ = false;
  bool disconnect_seen_ = false;
  int64_t attempt_start_ms_ = 0;
  int64_t wait_from_ms_ = 0;
  int failures_ = 0;
};

}  // namespace core
