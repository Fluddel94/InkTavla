#include "core/secrets.h"

#if defined(ARDUINO)
#include <Preferences.h>
#endif

namespace core {

namespace {

constexpr const char* kKnown[] = {kSecretCalendarUrl};

bool startsWithNoCase(std::string_view text, std::string_view prefix) {
  if (text.size() < prefix.size()) return false;
  for (size_t i = 0; i < prefix.size(); i++) {
    char c = text[i];
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if (c != prefix[i]) return false;
  }
  return true;
}

bool endsWith(std::string_view text, std::string_view suffix) {
  return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

#if defined(ARDUINO)
constexpr char kNamespace[] = "secrets";
#endif

}  // namespace

bool isKnownSecret(std::string_view name) {
  for (const char* k : kKnown) {
    if (name == k) return true;
  }
  return false;
}

bool checkSecret(std::string_view name, std::string_view value, std::string& reason) {
  if (!isKnownSecret(name)) {
    reason = "unknown secret";
    return false;
  }
  if (value.empty() || value.size() > kMaxSecretBytes) {
    reason = "must be 1 to " + std::to_string(kMaxSecretBytes) + " characters";
    return false;
  }
  for (char c : value) {
    if (c <= ' ' || c > '~') {
      reason = "contains spaces or characters other than printable ASCII";
      return false;
    }
  }
  if (endsWith(name, "_url")) {
    const size_t scheme = startsWithNoCase(value, "https://")    ? 8
                          : startsWithNoCase(value, "webcal://") ? 9
                                                                 : 0;
    if (scheme == 0) {
      reason = "must start with https:// or webcal://";
      return false;
    }
    if (value.size() <= scheme) {
      reason = "address without a host";
      return false;
    }
  }
  return true;
}

std::string secretUrl(std::string_view value) {
  if (startsWithNoCase(value, "webcal://")) return "https://" + std::string(value.substr(9));
  return std::string(value);
}

#if defined(ARDUINO)
bool NvsSecretStore::write(const char* name, const std::string& value) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;
  const bool ok = prefs.putString(name, value.c_str()) == value.size();
  prefs.end();
  return ok;
}

bool NvsSecretStore::erase(const char* name) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;
  if (prefs.isKey(name)) prefs.remove(name);
  prefs.end();
  return true;
}

bool NvsSecretStore::read(const char* name, std::string& value) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) return false;  // namespace not created yet
  const bool found = prefs.isKey(name);
  if (found) value = prefs.getString(name, "").c_str();
  prefs.end();
  return found;
}
#endif

}  // namespace core
