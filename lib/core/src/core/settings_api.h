#pragma once

// Saving from the settings page: the page sends the whole configuration
// (built from its forms) and new values for secrets in one request:
//
//   {"config": {...}, "secrets": {"calendar_url": "https://..."}, "force": false}
//
// A secret set to null is removed; a secret that is missing stays as it is.
// The configuration is checked with the same rules as over USB
// (core/config_command.h). If problems are found, nothing is saved and
// they are returned, unless "force" is true (then invalid values fall back
// to their defaults, as with a file sent over USB). A secret that fails
// checkSecret() is never saved. Secrets are saved before the file, so a
// failure leaves the old file. A calendar address in the configuration
// itself is removed (stripSecrets()). The file is written to a temporary
// file first and then renamed, like over USB.

#include <string>

#include "core/config.h"
#include "core/config_command.h"
#include "core/secrets.h"

namespace core {

struct SettingsSaveResult {
  int status = 200;  // HTTP status: 200 saved, 422 problems, 400 bad request, 500 write error
  std::string json;  // {"saved": bool, "issues": [{"path": ..., "message": ...}], "error": ...}
  bool restart = false;
};

SettingsSaveResult saveSettings(const std::string& body, ConfigStore& store,
                                const ConfigValidator& validate, SecretStore* secrets);

// Resets the settings: an empty configuration (all defaults) and no
// calendar address (secrets). Wi-Fi stays, so the device can still be
// reached. Nothing is changed if the secret cannot be cleared.
SettingsSaveResult resetSettings(ConfigStore& store, SecretStore& secrets);

// The stored configuration as JSON text for the page and the backup,
// without a calendar address (stripSecrets()); "{}" if there is none or it
// cannot be read (then *problem says why).
std::string storedConfig(ConfigStore& store, std::string* problem = nullptr);

}  // namespace core
