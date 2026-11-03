#pragma once

// Secrets of widgets (e.g. the private iCal address of a calendar): entered
// over USB (INKTAVLA SECRET SET, see core/config_command.h), kept only in
// the device's protected storage (NVS, like the Wi-Fi credentials), never
// part of /config.json, never sent back and never printed.

#include <cstddef>
#include <string>
#include <string_view>

namespace core {

// Private iCal address of the calendar widget.
inline constexpr char kSecretCalendarUrl[] = "calendar_url";

inline constexpr size_t kMaxSecretBytes = 1024;

// True for the names above (NVS keys, at most 15 characters).
bool isKnownSecret(std::string_view name);

// Checks a value before it is saved: printable ASCII without spaces; names
// ending in "_url" need an https:// (or webcal://) address, since services
// on the internet are only requested over HTTPS. False with a reason.
bool checkSecret(std::string_view name, std::string_view value, std::string& reason);

// An address as it is requested: webcal:// becomes https://.
std::string secretUrl(std::string_view value);

class SecretStore {
 public:
  virtual ~SecretStore() = default;
  virtual bool write(const char* name, const std::string& value) = 0;
  virtual bool erase(const char* name) = 0;
  virtual bool read(const char* name, std::string& value) = 0;
  bool has(const char* name) {
    std::string value;
    return read(name, value) && !value.empty();
  }
};

#if defined(ARDUINO)
// NVS namespace "secrets", one key per name (never rename them: existing
// devices would lose their settings).
class NvsSecretStore : public SecretStore {
 public:
  bool write(const char* name, const std::string& value) override;
  bool erase(const char* name) override;
  bool read(const char* name, std::string& value) override;
};
#endif

}  // namespace core
