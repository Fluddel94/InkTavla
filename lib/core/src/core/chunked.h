#pragma once

// Removes the chunk lengths of an HTTP body sent with
// "Transfer-Encoding: chunked" ("1a\r\n<26 bytes>\r\n...0\r\n\r\n"). The MVG
// API normally answers without length and without chunks; this keeps the
// reader working if a server does use chunks.

#include <cstdint>

namespace core {

class ChunkedDecoder {
 public:
  static constexpr int kNoByte = -1;

  // Feeds one byte as received; returns the body byte, or kNoByte if the
  // byte belongs to the chunk framing.
  int feed(uint8_t c);

  // The last chunk (length 0) has been seen: the body is complete.
  bool finished() const { return state_ == State::kDone; }

  // A chunk length line was malformed.
  bool failed() const { return failed_; }

 private:
  enum class State { kSize, kExtension, kData, kDataCr, kDataLf, kDone };
  State state_ = State::kSize;
  uint32_t left_ = 0;
  bool digits_ = false;
  bool failed_ = false;
};

}  // namespace core
