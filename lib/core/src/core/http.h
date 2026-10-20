#pragma once

// HTTPS requests with certificate check.
//
// Services on the internet are always requested over HTTPS and the
// certificate is checked against the certificate bundle of the ESP32 core
// (the Mozilla root certificates that ESP-IDF builds into mbedTLS, enabled
// in the precompiled framework of pioarduino 55.03.312-1 with
// CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL). Any server with a
// certificate from a common CA works, and a change of CA does not break it.
//
// JSON responses are parsed directly from the stream with an ArduinoJson
// filter and never held as a string. The body reader copes with responses
// without length, chunked or not (the MVG API sends neither length nor
// chunks; the end is the end of the connection).

#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace core {

// Connect, TLS handshake and reading: a handshake takes longer than 5 s
// now and then (HTTP -11 in the MVG departure display).
inline constexpr uint32_t kHttpTimeoutMs = 10000;

struct HttpResult {
  int status = 0;          // HTTP status, negative: error of the HTTP client
  size_t bytes = 0;        // body bytes read
  bool timed_out = false;  // reading stopped by kHttpTimeoutMs
  std::string error;       // JSON error, if any
};

#if defined(ARDUINO)
// GET url and read the JSON body into doc, keeping only the fields in
// filter. True on status 200 and valid JSON.
bool fetchJsonStream(const std::string& url, JsonDocument& doc, const JsonDocument& filter,
                     HttpResult* result = nullptr);
#endif

}  // namespace core
