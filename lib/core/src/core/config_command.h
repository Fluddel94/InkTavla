#pragma once

// Configuration over USB until the settings page exists: text commands on
// the serial line, next to Improv (Improv packets start with "IMPROV",
// these commands with "INKTAVLA "; everything else is ignored).
//
//   INKTAVLA CONFIG PUT <bytes> <crc32>   then exactly <bytes> bytes of JSON
//   INKTAVLA CONFIG GET                   the stored /config.json
//   INKTAVLA INFO                         firmware version and memory
//   INKTAVLA LOG                          the system log (core/syslog.h)
//   INKTAVLA SECRET SET <name> <bytes> <crc32>   then exactly <bytes> bytes
//   INKTAVLA SECRET CLEAR <name>
//
// Secrets (core/secrets.h, e.g. "calendar_url") are kept in NVS, never in
// the configuration file; there is no command to read them back, and
// replies never repeat them. Saving or clearing one restarts the device.
// A calendar address inside a configuration file is removed when it is
// saved and when it is read (core::stripSecrets()).
//
// Replies are lines starting with "INKTAVLA ": "OK ...", "ERR <reason>",
// "ISSUE <path>: <message>" (problems found in a new configuration),
// "DATA <bytes> <crc32>" followed by the file, "INFO ...", "LOG <lines>"
// followed by the lines of the log and "OK". The CRC-32 is
// the usual one (zlib, PNG) in hexadecimal. tools/inktavla_config.py
// implements the PC side.

#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "core/config.h"
#include "core/secrets.h"

namespace core {

uint32_t crc32(const uint8_t* data, size_t length, uint32_t crc = 0);

class CommandParser {
 public:
  static constexpr size_t kMaxLine = 96;
  static constexpr int64_t kPayloadTimeoutMs = 5000;

  struct Event {
    enum class Kind {
      kNone,
      kConfigPut,
      kConfigGet,
      kInfo,
      kLog,
      kSecretSet,
      kSecretClear,
      kError
    };
    Kind kind = Kind::kNone;
    std::string payload;  // kConfigPut: the received file, kSecretSet: the value (CRC checked)
    std::string name;     // kSecretSet, kSecretClear: name of the secret
    std::string error;    // kError: reason
  };

  // Feeds one received byte.
  Event feed(uint8_t b, int64_t now_ms);

  // Reports an interrupted transfer (no byte for kPayloadTimeoutMs).
  Event poll(int64_t now_ms);

  // True while the bytes of a PUT are being received; they must not be
  // passed to other protocols.
  bool receivingPayload() const { return payload_left_ > 0; }

 private:
  Event line(const std::string& text);

  std::string line_;
  bool overflow_ = false;
  std::string payload_;
  std::string payload_name_;  // empty: configuration file, else the secret
  size_t payload_left_ = 0;
  uint32_t payload_crc_ = 0;
  int64_t last_byte_ms_ = 0;
};

// File access for the commands (LittleFS on the device, memory in tests).
class ConfigStore {
 public:
  virtual ~ConfigStore() = default;
  virtual bool write(const char* path, const std::string& data) = 0;
  virtual bool rename(const char* from, const char* to) = 0;
  virtual bool read(const char* path, std::string& data) = 0;
};

// Reports the problems of a configuration (core and widget sections).
using ConfigValidator = std::function<void(JsonObjectConst root, ConfigIssues& issues)>;

struct CommandReply {
  std::string text;      // lines to send, each ending with "\n"
  bool restart = false;  // a new configuration was saved
};

// Carries out a command. A new configuration is only saved if it is valid
// JSON (an object, at most kMaxConfigBytes); it is written to a temporary
// file first and then renamed, so the old file stays complete until the new
// one is. Secrets need `secrets` and are checked with checkSecret().
CommandReply handleCommand(const CommandParser::Event& event, ConfigStore& store,
                           const ConfigValidator& validate, const std::string& info,
                           SecretStore* secrets = nullptr);

}  // namespace core
