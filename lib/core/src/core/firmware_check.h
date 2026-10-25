#pragma once

// Checks a firmware file while it is uploaded on the settings page (from
// the MVG departure display): it must be an ESP32-S3 app image (first byte
// 0xE9, chip ID 9 at offset 12) and contain kFirmwareMarker, which every
// InkTavla firmware contains (it is part of this check). So a wrong file
// (another project, a file system image, a firmware for another chip) is
// refused before the device restarts with it.
//
// After the restart the new firmware is only confirmed once it runs
// properly (see firmwareConfirmed()); if it crashes before, the bootloader
// goes back to the previous one.

#include <cstddef>
#include <cstdint>

namespace core {

// "InkTavla/firmware-image". Never change this text: older devices would
// refuse newer firmware. Its first character must not appear again in it
// (see FirmwareCheck). Defined in firmware_check.cpp, kept in the image.
extern const char kFirmwareMarker[];
inline constexpr size_t kFirmwareMarkerLength = 23;

class FirmwareCheck {
 public:
  static constexpr uint8_t kImageMagic = 0xE9;
  static constexpr uint16_t kChipIdEsp32S3 = 9;

  // Feeds the next part of the file. False as soon as the header shows it
  // is no ESP32-S3 app image.
  bool feed(const uint8_t* data, size_t length);

  bool headerOk() const { return header_ok_; }
  bool markerFound() const { return found_; }
  // The whole file was fed: valid header and marker found.
  bool complete() const { return header_ok_ && found_; }

 private:
  uint8_t header_[16] = {};
  size_t header_bytes_ = 0;
  bool header_checked_ = false;
  bool header_ok_ = false;
  size_t matched_ = 0;  // characters of the marker matched so far
  bool found_ = false;
};

// True once a new firmware may be confirmed: it has been running for 30 s
// with Wi-Fi, or for 10 minutes without (so a router that is off does not
// make it go back, but a firmware that cannot connect at all is noticed by
// the user before that).
bool firmwareConfirmed(int64_t uptime_ms, bool wifi_connected);

}  // namespace core
