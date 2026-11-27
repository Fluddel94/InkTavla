#if defined(ARDUINO)

#include "core/http.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>

#include "core/chunked.h"
#include "core/fetch_cancel.h"
#include "core/syslog.h"

namespace core {

namespace {

// Body of a response as a Stream for ArduinoJson: waits for data up to the
// timeout, ends after Content-Length bytes or with the connection, removes
// chunk lengths if needed. Stopping at the length means the closed
// connection is never read (some servers, e.g. Google Calendar, close it
// without a TLS close notification: "ssl_client ... (-76)" in the log).
class HttpBodyStream : public Stream {
 public:
  // length: Content-Length, negative if unknown. limit_ms and max_bytes
  // (0: none) end the body after that time or that many bytes.
  HttpBodyStream(Client& client, bool chunked, int64_t length, uint32_t timeout_ms,
                 uint32_t limit_ms, size_t max_bytes)
      : in_(client),
        chunked_(chunked),
        length_(chunked ? -1 : length),
        timeout_ms_(timeout_ms),
        limit_ms_(limit_ms),
        max_bytes_(max_bytes),
        start_ms_(millis()) {}

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
  // Reading stopped for a button press (core/fetch_cancel.h).
  bool cancelled() const { return cancelled_; }
  int64_t length() const { return length_; }    // negative if unknown
  bool tooLong() const { return too_long_; }    // limit_ms reached
  bool tooLarge() const { return too_large_; }  // max_bytes reached

 private:
  // Next byte from the network, -1 at the end, after the timeout or when
  // a button press waits (asked every kCancelCheckBytes and while waiting).
  int rawByte() {
    if (++since_check_ >= kCancelCheckBytes) {
      since_check_ = 0;
      if (stop() || overTime()) return -1;
    }
    const uint32_t start = millis();
    uint32_t waited = 0;
    while (true) {
      const int c = in_.read();
      if (c >= 0) return c;
      if (!in_.connected() && in_.available() <= 0) return -1;
      if (millis() - start >= timeout_ms_) {
        timed_out_ = true;
        return -1;
      }
      delay(1);
      if (++waited % 50 == 0 && (stop() || overTime())) return -1;
    }
  }

  bool overTime() {
    if (limit_ms_ > 0 && millis() - start_ms_ >= limit_ms_) too_long_ = true;
    return too_long_;
  }

  bool stop() {
    if (!cancelled_ && fetchShouldStop()) cancelled_ = true;
    return cancelled_;
  }

  static constexpr uint32_t kCancelCheckBytes = 512;

  int nextBodyByte() {
    while (true) {
      if (length_ >= 0 && raw_bytes_ >= length_) {
        finished_ = true;
        return -1;
      }
      if (max_bytes_ > 0 && raw_bytes_ >= static_cast<int64_t>(max_bytes_)) {
        too_large_ = true;
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
  uint32_t limit_ms_;
  size_t max_bytes_;
  uint32_t start_ms_;
  bool too_long_ = false;
  bool too_large_ = false;
  ChunkedDecoder decoder_;
  bool finished_ = false;
  bool timed_out_ = false;
  bool cancelled_ = false;
  uint32_t since_check_ = 0;
  int peeked_ = -1;
  size_t bytes_ = 0;
};

// Common part of the requests: TLS with the certificate bundle, timeouts,
// HTTP/1.0 (no chunks if the server honours it). read gets the body of a
// response with status 200.
void configure(NetworkClientSecure& client, HTTPClient& http) {
  client.useBuiltinCACertBundle();  // certificate check with the core's CA bundle
  client.setHandshakeTimeout(kHttpTimeoutMs / 1000);
  http.setConnectTimeout(kHttpTimeoutMs);
  http.setTimeout(kHttpTimeoutMs);
  http.useHTTP10(true);  // no chunks if the server honours it
}

// Why a request failed, for the system log (core/syslog.h): fixed texts of
// the firmware and its libraries and numbers only, never the URL.
void noteProblem(NetworkClientSecure& client, const HttpResult& r) {
  if (r.status > 0 && r.status != 200) {
    noteFetchProblem("HTTP %d", r.status);
  } else if (r.timed_out) {
    noteFetchProblem("timed out after %u bytes", static_cast<unsigned>(r.bytes));
  } else if (!r.error.empty()) {
    noteFetchProblem("%s", r.error.c_str());
  } else if (r.status < 0) {
    // E.g. a failed TLS handshake or certificate check (mbedTLS text).
    char tls[48] = "";
    client.lastError(tls, sizeof(tls));
    noteFetchProblem("%s%s%s", HTTPClient::errorToString(r.status).c_str(), tls[0] ? ": " : "",
                     tls);
  } else {
    noteFetchProblem("HTTP %d", r.status);
  }
}

// limit_ms, max_bytes: see HttpBodyStream (0: none).
bool get(const std::string& url, bool follow_redirects, uint32_t limit_ms, size_t max_bytes,
         const std::function<bool(HttpBodyStream&, HttpResult&)>& read, HttpResult* result) {
  HttpResult r;
  NetworkClientSecure client;
  HTTPClient http;
  configure(client, http);
  if (follow_redirects) http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  const char* header_keys[] = {"Transfer-Encoding"};
  http.collectHeaders(header_keys, 1);

  bool ok = false;
  if (fetchShouldStop()) {
    // A button press waits: do not start (the caller repeats it later).
    r.error = "cancelled";
    if (result != nullptr) *result = r;
    return false;
  }
  if (!http.begin(client, url.c_str())) {
    r.status = -1;
  } else {
    r.status = http.GET();
    if (r.status == 200) {
      const bool chunked = http.header("Transfer-Encoding").equalsIgnoreCase("chunked");
      HttpBodyStream body(client, chunked, http.getSize(), kHttpTimeoutMs, limit_ms, max_bytes);
      ok = read(body, r);
      r.bytes = body.bytes();
      r.timed_out = body.timedOut();
      if (body.tooLong() || body.tooLarge()) {
        ok = false;
        r.error = body.tooLong() ? "took too long" : "response too large";
      }
      if (body.cancelled()) {
        ok = false;
        r.error = "cancelled";
      }
    }
    http.end();
  }
  if (!ok && r.error == "cancelled") {
    Serial.printf("HTTP: stopped for a button press after %u bytes\n",
                  static_cast<unsigned>(r.bytes));
  } else if (!ok) {
    // No URL in the log: it may contain secrets (e.g. a private iCal address).
    Serial.printf("HTTP: request failed, status %d%s%s%s\n", r.status,
                  r.timed_out ? ", timed out" : "", r.error.empty() ? "" : ", ", r.error.c_str());
    noteProblem(client, r);
  }
  if (result != nullptr) *result = r;
  return ok;
}

}  // namespace

bool fetchJsonStream(const std::string& url, JsonDocument& doc, const JsonDocument& filter,
                     HttpResult* result) {
  return get(
      url, false, kJsonLimitMs, kMaxJsonBytes,
      [&](HttpBodyStream& body, HttpResult& r) {
        const DeserializationError error =
            deserializeJson(doc, body, DeserializationOption::Filter(filter));
        if (error) r.error = std::string("JSON ") + error.c_str();
        return !error;
      },
      result);
}

bool fetchStream(const std::string& url, const BodySink& sink, size_t max_bytes, HttpResult* result,
                 const std::function<void(int64_t length)>& on_length) {
  return get(
      url, true, 0, 0,  // the caller limits the size (max_bytes) and time (sink)
      [&](HttpBodyStream& body, HttpResult& r) {
        if (on_length) on_length(body.length());
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

int fetchRedirect(const std::string& url, std::string& location) {
  location.clear();
  NetworkClientSecure client;
  HTTPClient http;
  configure(client, http);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  if (!http.begin(client, url.c_str())) return -1;
  const int status = http.GET();
  location = http.getLocation().c_str();
  http.end();
  if (status <= 0) Serial.printf("HTTP: request failed, status %d\n", status);
  return status;
}

}  // namespace core

#endif
