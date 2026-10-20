#include "core/url.h"

#include <cctype>

namespace core {

std::string urlEncode(std::string_view text) {
  static const char kHex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(text.size());
  for (char ch : text) {
    const auto c = static_cast<unsigned char>(ch);
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += static_cast<char>(c);
    } else {
      out += '%';
      out += kHex[c >> 4];
      out += kHex[c & 0x0F];
    }
  }
  return out;
}

}  // namespace core
