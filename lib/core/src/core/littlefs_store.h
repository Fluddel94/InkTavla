#pragma once

// Configuration files in LittleFS (mounted by main.cpp), for the commands
// over USB and the settings page.

#if defined(ARDUINO)

#include <string>

#include "core/config_command.h"

namespace core {

class LittleFsStore : public ConfigStore {
 public:
  bool write(const char* path, const std::string& data) override;
  // littlefs replaces an existing file atomically; the fallback (remove,
  // then rename) is only for file systems that refuse that.
  bool rename(const char* from, const char* to) override;
  bool read(const char* path, std::string& data) override;
};

}  // namespace core

#endif
