#pragma once

// Allocator for large JSON documents (API responses): PSRAM on the device,
// so the internal heap stays free for TLS and Wi-Fi; the normal heap on the
// PC.

#include <ArduinoJson.h>

namespace core {

ArduinoJson::Allocator* jsonAllocator();

}  // namespace core
