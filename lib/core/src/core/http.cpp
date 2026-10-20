#if defined(ARDUINO)

#include "core/http.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>

#include "core/chunked.h"

namespace core {

namespace {

// Body of a response as a Stream for ArduinoJson: waits for data up to the
// timeout, ends with the connection, removes chunk lengths if needed.
class HttpBodyStream : public Stream {
 public:
  HttpBodyStream(Client& client, bool chunked, uint32_t timeout_ms)
      : in_(client), chunked_(chunked), timeout_ms_(timeout_ms) {}

  int read() override {
    const int c = peek();
    peeked_ = -1;
    if (c >= 0) bytes_++;
    return c;
  }
  int peek() override {
    if (peeked_ < 0 && !finished_) peeked_ = nextBodyByte();
    return peeked_;
  }
  int available() override { return (peeked_ >= 0 || !finished_) ? 1 : 0; }
  size_t write(uint8_t) override { return 0; }

  size_t bytes() const { return bytes_; }
  bool timedOut() const { return timed_out_; }

 private:
  // Next byte from the network, -1 at the end or after the timeout.
  int rawByte() {
    const uint32_t start = millis();
    while (true) {
      const int c = in_.read();
      if (c >= 0) return c;
      if (!in_.connected() && in_.available() <= 0) return -1;
      if (millis() - start >= timeout_ms_) {
        timed_out_ = true;
        return -1;
      }
      delay(1);
    }
  }

  int nextBodyByte() {
    while (true) {
      const int c = rawByte();
      if (c < 0) {
        finished_ = true;
        return -1;
      }
      if (!chunked_) return c;
      const int body = decoder_.feed(static_cast<uint8_t>(c));
      if (decoder_.finished() || decoder_.failed()) {
        finished_ = true;
        return -1;
      }
      if (body != ChunkedDecoder::kNoByte) return body;
    }
  }

  Client& in_;
  bool chunked_;
  uint32_t timeout_ms_;
  ChunkedDecoder decoder_;
  bool finished_ = false;
  bool timed_out_ = false;
  int peeked_ = -1;
  size_t bytes_ = 0;
};

}  // namespace

bool fetchJsonStream(const std::string& url, JsonDocument& doc, const JsonDocument& filter,
                     HttpResult* result) {
  HttpResult r;
  NetworkClientSecure client;
  client.useBuiltinCACertBundle();  // certificate check with the core's CA bundle
  client.setHandshakeTimeout(kHttpTimeoutMs / 1000);

  HTTPClient http;
  http.setConnectTimeout(kHttpTimeoutMs);
  http.setTimeout(kHttpTimeoutMs);
  http.useHTTP10(true);  // no chunks if the server honours it
  const char* header_keys[] = {"Transfer-Encoding"};
  http.collectHeaders(header_keys, 1);

  bool ok = false;
  if (!http.begin(client, url.c_str())) {
    r.status = -1;
  } else {
    r.status = http.GET();
    if (r.status == 200) {
      const bool chunked = http.header("Transfer-Encoding").equalsIgnoreCase("chunked");
      HttpBodyStream body(client, chunked, kHttpTimeoutMs);
      const DeserializationError error =
          deserializeJson(doc, body, DeserializationOption::Filter(filter));
      r.bytes = body.bytes();
      r.timed_out = body.timedOut();
      if (error) {
        r.error = error.c_str();
      } else {
        ok = true;
      }
    }
    http.end();
  }
  if (!ok) {
    // No URL in the log: it may contain secrets (e.g. a private iCal address).
    Serial.printf("HTTP: request failed, status %d%s%s\n", r.status,
                  r.error.empty() ? "" : ", JSON ", r.error.c_str());
  }
  if (result != nullptr) *result = r;
  return ok;
}

}  // namespace core

#endif
