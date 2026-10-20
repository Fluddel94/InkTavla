#pragma once

// Freshness of a widget's data: decides whether the display shows the data
// as it is, with an age hint, or a message instead.
//
// Short disturbances (API or Wi-Fi down for up to kGraceMs) are bridged
// without any hint: the last data stays on the display. After that, old
// data is shown with "as of <time of the last success>"; a widget that
// never had data shows a message.

#include <cstdint>

namespace core {

enum class DataState {
  kWaiting,   // no data yet, first attempts still within the grace period
  kFresh,     // data from the last successful update (or failing < grace)
  kStale,     // data, but updates have failed for kGraceMs or longer
  kNoData,    // never had data, failing for kGraceMs or longer
  kNotSetUp,  // the widget lacks settings; nothing is fetched
};

class Freshness {
 public:
  static constexpr int64_t kGraceMs = 60000;

  void success(int64_t now_ms, int64_t wall_s);
  void failure(int64_t now_ms);
  void setNotSetUp(bool not_set_up) { not_set_up_ = not_set_up; }

  DataState state(int64_t now_ms) const;

  // Wall-clock time (Unix seconds) of the last success, 0 if none or if the
  // clock was not synchronised then.
  int64_t lastSuccessWall() const { return last_success_wall_s_; }

 private:
  bool not_set_up_ = false;
  bool has_data_ = false;
  bool failing_ = false;
  int64_t failing_since_ms_ = 0;
  int64_t last_success_wall_s_ = 0;
};

}  // namespace core
