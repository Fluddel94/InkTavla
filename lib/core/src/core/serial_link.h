#pragma once

// The USB serial line of the device: Improv (Wi-Fi credentials from the
// browser) and the configuration commands (core/config_command.h) on the
// same line. Runs in its own FreeRTOS task, so the device also answers while
// loop() is busy with a display refresh or a request.

#if defined(ARDUINO)

#include <functional>
#include <string>

#include "core/config_command.h"
#include "core/wifi.h"

namespace core {

struct SerialLinkOptions {
  const char* firmware_name = "InkTavla";
  const char* firmware_version = "";
  const char* device_name = "InkTavla";
  // Checks a new configuration before it is saved (core and widgets).
  ConfigValidator validate;
  // Text for "INKTAVLA INFO".
  std::function<std::string()> info;
};

// Starts the task (once in setup(), after Serial.begin()).
void startSerialLink(Wifi& wifi, SerialLinkOptions options);

}  // namespace core

#endif
