#if defined(ARDUINO)

#include "core/http.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>

#include "core/chunked.h"

namespace core {

namespace {

// Body of a response as a Stream for ArduinoJson: waits for data up to the
// timeout, ends after Content-Length bytes or with the connection, removes
// chunk lengths if needed. Stopping at the length means the closed
// connection is never read (some servers, e.g. Google Calendar, close it
// without a TLS close notification: "ssl_client ... (-76)" in the log).
class HttpBodyStream : public Stream {
 public:
  // length: Content-Length, negative if unknown.
  HttpBodyStream(Client& client, bool chunked, int64_t length, uint32_t timeout_ms)
      : in_(client), chunked_(chunked), length_(chunked ? -1 : length), timeout_ms_(timeout_ms) {}

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
      if (length_ >= 0 && raw_bytes_ >= length_) {
        finished_ = true;
        return -1;
      }
      const int c = rawByte();
      raw_bytes_++;
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
  int64_t length_;
  int64_t raw_bytes_ = 0;
  uint32_t timeout_ms_;
  ChunkedDecoder decoder_;
  bool finished_ = false;
  bool timed_out_ = false;
  int peeked_ = -1;
  size_t bytes_ = 0;
};

// Common part of the requests: TLS with the certificate bundle, timeouts,
// HTTP/1.0 (no chunks if the server honours it). read gets the body of a
// response with status 200.
bool get(const std::string& url, bool follow_redirects,
         const std::function<bool(HttpBodyStream&, HttpResult&)>& read, HttpResult* result) {
  HttpResult r;
  NetworkClientSecure client;
  client.useBuiltinCACertBundle();  // certificate check with the core's CA bundle
  client.setHandshakeTimeout(kHttpTimeoutMs / 1000);

  HTTPClient http;
  http.setConnectTimeout(kHttpTimeoutMs);
  http.setTimeout(kHttpTimeoutMs);
  http.useHTTP10(true);  // no chunks if the server honours it
  if (follow_redirects) http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  const char* header_keys[] = {"Transfer-Encoding"};
  http.collectHeaders(header_keys, 1);

  bool ok = false;
  if (!http.begin(client, url.c_str())) {
    r.status = -1;
  } else {
    r.status = http.GET();
    if (r.status == 200) {
      const bool chunked = http.header("Transfer-Encoding").equalsIgnoreCase("chunked");
      HttpBodyStream body(client, chunked, http.getSize(), kHttpTimeoutMs);
      ok = read(body, r);
      r.bytes = body.bytes();
      r.timed_out = body.timedOut();
    }
    http.end();
  }
  if (!ok) {
    // No URL in the log: it may contain secrets (e.g. a private iCal address).
    Serial.printf("HTTP: request failed, status %d%s%s%s\n", r.status,
                  r.timed_out ? ", timed out" : "", r.error.empty() ? "" : ", ", r.error.c_str());
  }
  if (result != nullptr) *result = r;
  return ok;
}

}  // namespace

bool fetchJsonStream(const std::string& url, JsonDocument& doc, const JsonDocument& filter,
                     HttpResult* result) {
  return get(
      url, false,
      [&](HttpBodyStream& body, HttpResult& r) {
        const DeserializationError error =
            deserializeJson(doc, body, DeserializationOption::Filter(filter));
        if (error) r.error = std::string("JSON ") + error.c_str();
        return !error;
      },
      result);
}

bool fetchStream(const std::string& url, const BodySink& sink, size_t max_bytes,
                 HttpResult* result) {
  return get(
      url, true,
      [&](HttpBodyStream& body, HttpResult& r) {
        char buffer[512];
        size_t total = 0;
        while (true) {
          size_t n = 0;
          int c = -1;
          while (n < sizeof(buffer) && (c = body.read()) >= 0) buffer[n++] = static_cast<char>(c);
          total += n;
          if (total > max_bytes) {
            r.error = "response too large";
            return false;
          }
          if (n > 0 && !sink(buffer, n)) {
            r.error = "stopped by the reader";
            return false;
          }
          // A body without length ends with the connection; one that stops
          // arriving is incomplete.
          if (c < 0) return !body.timedOut();
        }
      },
      result);
}

}  // namespace core

#endif
