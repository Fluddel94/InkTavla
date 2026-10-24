#include "core/access_notice.h"

namespace core {

AccessMode wifiNoticeMode(bool credentials_saved, bool connected_since_start,
                          int64_t ms_since_start) {
  if (!credentials_saved) return AccessMode::kWifiNotSetUp;
  if (!connected_since_start && ms_since_start >= kWifiNoticeDelayMs) {
    return AccessMode::kWifiNotConnected;
  }
  return AccessMode::kNone;
}

}  // namespace core
