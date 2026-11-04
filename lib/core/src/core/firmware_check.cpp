#include "core/firmware_check.h"

namespace core {

// "used": the text must stay in the firmware even where the compiler could
// do without the array.
__attribute__((used)) const char kFirmwareMarker[] = "InkTavla/firmware-image";
static_assert(sizeof("InkTavla/firmware-image") - 1 == kFirmwareMarkerLength, "length");

bool FirmwareCheck::feed(const uint8_t* data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    const uint8_t b = data[i];
    if (!header_checked_) {
      header_[header_bytes_++] = b;
      if (header_bytes_ == sizeof(header_)) {
        header_checked_ = true;
        const uint16_t chip = static_cast<uint16_t>(header_[12] | (header_[13] << 8));
        uint32_t app_desc = 0;
        for (int k = 3; k >= 0; k--) app_desc = (app_desc << 8) | header_[kAppDescOffset + k];
        header_ok_ =
            header_[0] == kImageMagic && chip == kChipIdEsp32S3 && app_desc == kAppDescMagic;
      }
    }
    if (header_checked_ && !header_ok_) return false;
    if (found_) continue;
    // The first character of the marker does not appear again in it, so a
    // mismatch can restart at this byte.
    // Read through a volatile pointer, so the text stays in the image.
    const volatile char* marker = kFirmwareMarker;
    if (b == static_cast<uint8_t>(marker[matched_])) {
      if (++matched_ == kFirmwareMarkerLength) found_ = true;
    } else {
      matched_ = b == static_cast<uint8_t>(marker[0]) ? 1 : 0;
    }
  }
  return !header_checked_ || header_ok_;
}

bool firmwareConfirmed(int64_t uptime_ms, bool wifi_connected) {
  return (wifi_connected && uptime_ms >= 30000) || uptime_ms >= 10 * 60 * 1000;
}

}  // namespace core
