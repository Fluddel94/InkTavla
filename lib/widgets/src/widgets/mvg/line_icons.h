#pragma once

// MVV line symbols from the MVG departure display (see line_icons.cpp for
// sources and notes).

#include <cstdint>
#include <string>

namespace widgets::mvg {

inline constexpr int kLineIconHeight = 16;

struct LineIcon {
  const char* label;    // as delivered by the MVG API: "S1", "U3", "19", "N17"
  uint8_t width;        // height is always kLineIconHeight
  const uint8_t* bits;  // XBM: rows of (width + 7) / 8 bytes, least significant bit first
};

// Symbol for a line label, nullptr if there is none (buses, regional trains,
// unknown lines: they get a generated frame instead).
const LineIcon* findLineIcon(const std::string& label);

}  // namespace widgets::mvg
