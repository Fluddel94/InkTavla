#pragma once

// MVV line symbols from the MVG departure display (see line_icons.cpp for
// sources and notes).

#include <cstdint>
#include <string>

#include "ui/canvas.h"

namespace widgets::mvg {

inline constexpr int kLineIconHeight = 16;

// Width every symbol is drawn with: the widest original (the S-Bahn
// symbols); U-Bahn, tram and night tram symbols get more black space on
// both sides.
inline constexpr int kLineIconWidth = 36;

struct LineIcon {
  const char* label;    // as delivered by the MVG API: "S1", "U3", "19", "N17"
  uint8_t width;        // height is always kLineIconHeight
  const uint8_t* bits;  // XBM: rows of (width + 7) / 8 bytes, least significant bit first
};

// Symbol for a line label, nullptr if there is none (buses, regional trains,
// unknown lines: they get a generated symbol instead).
const LineIcon* findLineIcon(const std::string& label);

// Draws the symbol kLineIconWidth wide with its top left corner at (x, y):
// the S-Bahn symbols as they are, the box symbols widened with black.
void drawLineIcon(ui::Canvas& canvas, int x, int y, const LineIcon& icon);

// Symbol of any line, as in the MVG departure display: the MVV symbol if
// there is one (never for buses), otherwise a square frame with the name in
// the pixel digits of the tram symbols (frame 2 px, 1 px for long names;
// other characters in Roboto Condensed Bold 15). At least kLineIconWidth
// wide, wider only for long names. Returns the width.
int lineSymbolWidth(const std::string& label, bool bus);
int drawLineSymbol(ui::Canvas& canvas, int x, int y, const std::string& label, bool bus);

}  // namespace widgets::mvg
