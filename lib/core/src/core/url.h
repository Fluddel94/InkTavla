#pragma once

#include <string>
#include <string_view>

namespace core {

// Percent-encoding for a URL query parameter: letters, digits and "-_.~"
// stay, everything else (UTF-8 byte by byte) becomes %XX.
std::string urlEncode(std::string_view text);

}  // namespace core
