#pragma once

// Writes a new firmware into the second OTA slot while it arrives, the same
// way for a file uploaded on the settings page and for a release downloaded
// with the update button: every part goes through FirmwareCheck (ESP32-S3
// app image with the InkTavla marker, core/firmware_check.h) before it is
// written; at the end the image is verified by the Update library and set
// as the next boot partition. After the restart it is confirmed once it
// runs (main.cpp); otherwise the bootloader goes back to the old firmware.
//
// On the first problem the update is aborted and the old firmware stays
// the boot partition; error() says why (English, the page translates it).

#if defined(ARDUINO)

#include <cstddef>
#include <cstdint>
#include <string>

#include "core/firmware_check.h"

namespace core {

class FirmwareWriter {
 public:
  ~FirmwareWriter() { abort(); }

  // expected_size: total bytes if known, 0 otherwise (then the size of the
  // partition is the limit).
  bool begin(size_t expected_size = 0);
  bool write(const uint8_t* data, size_t length);
  // All bytes written: checks marker and image and sets the boot partition.
  bool finish();
  void abort();

  bool finished() const { return finished_; }  // finish() succeeded
  bool failed() const { return failed_; }
  const std::string& error() const { return error_; }
  size_t bytes() const { return bytes_; }

 private:
  bool fail(const char* error);

  FirmwareCheck check_;
  bool running_ = false;
  bool finished_ = false;
  bool failed_ = false;
  std::string error_;
  size_t bytes_ = 0;
};

}  // namespace core

#endif
