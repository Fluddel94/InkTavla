#include "core/fetch_cancel.h"

#include <atomic>
#include <mutex>
#include <utility>

namespace core {

namespace {

std::mutex check_mutex;  // the check is set once in setup(), read in the fetches
std::function<bool()> cancel_check;
std::atomic<bool> cancelled{false};

}  // namespace

void setFetchCancelCheck(std::function<bool()> check) {
  std::lock_guard<std::mutex> lock(check_mutex);
  cancel_check = std::move(check);
}

bool fetchStopPending() {
  std::function<bool()> check;
  {
    std::lock_guard<std::mutex> lock(check_mutex);
    check = cancel_check;
  }
  return check && check();
}

bool fetchShouldStop() {
  if (!fetchStopPending()) return false;
  cancelled = true;
  return true;
}

bool takeFetchCancelled() { return cancelled.exchange(false); }

}  // namespace core
