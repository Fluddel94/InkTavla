#include "core/chunked.h"

#include <cctype>

namespace core {

int ChunkedDecoder::feed(uint8_t c) {
  switch (state_) {
    case State::kSize:
      if (std::isxdigit(c)) {
        const uint32_t digit = std::isdigit(c) ? c - '0' : std::tolower(c) - 'a' + 10;
        if (left_ > 0x0FFFFFFF) failed_ = true;  // would overflow
        left_ = left_ * 16 + digit;
        digits_ = true;
        return kNoByte;
      }
      if (c == ';' || c == '\r' || c == ' ') {
        state_ = State::kExtension;
        return kNoByte;
      }
      if (c != '\n') {
        failed_ = true;
        return kNoByte;
      }
      [[fallthrough]];
    case State::kExtension:
      if (c != '\n') return kNoByte;  // extension or CR: skip up to LF
      if (!digits_) failed_ = true;
      digits_ = false;
      state_ = left_ == 0 ? State::kDone : State::kData;
      return kNoByte;
    case State::kData:
      if (--left_ == 0) state_ = State::kDataCr;
      return c;
    case State::kDataCr:
      state_ = State::kDataLf;
      if (c != '\r') failed_ = true;
      return kNoByte;
    case State::kDataLf:
      state_ = State::kSize;
      if (c != '\n') failed_ = true;
      return kNoByte;
    case State::kDone:
      return kNoByte;
  }
  return kNoByte;
}

}  // namespace core
