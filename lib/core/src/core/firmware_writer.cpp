#if defined(ARDUINO)

#include "core/firmware_writer.h"

#include <Arduino.h>
#include <Update.h>

#include "core/syslog.h"

namespace core {

bool FirmwareWriter::fail(const char* error) {
  if (!failed_) Serial.printf("Firmware update: %s\n", error);
  failed_ = true;
  if (error_.empty()) error_ = error;
  abort();
  return false;
}

bool FirmwareWriter::begin(size_t expected_size) {
  check_ = FirmwareCheck{};
  finished_ = false;
  failed_ = false;
  error_.clear();
  bytes_ = 0;
  if (!Update.begin(expected_size > 0 ? expected_size : UPDATE_SIZE_UNKNOWN, U_FLASH)) {
    return fail(expected_size > 0 ? "the file is too large" : "cannot start the update");
  }
  running_ = true;
  return true;
}

bool FirmwareWriter::write(const uint8_t* data, size_t length) {
  if (failed_ || !running_) return false;
  if (!check_.feed(data, length)) return fail("not an ESP32-S3 firmware image");
  if (Update.write(const_cast<uint8_t*>(data), length) != length) {
    return fail("cannot write to flash");
  }
  bytes_ += length;
  return true;
}

bool FirmwareWriter::finish() {
  if (failed_ || !running_) return false;
  if (!check_.complete()) return fail("not an InkTavla firmware");
  running_ = false;
  if (!Update.end(true)) return fail("the image is not valid");
  finished_ = true;
  logInfo("update", "firmware written (%u KB), checked", static_cast<unsigned>(bytes_ / 1024));
  return true;
}

void FirmwareWriter::abort() {
  if (running_ && Update.isRunning()) Update.abort();
  running_ = false;
}

}  // namespace core

#endif
