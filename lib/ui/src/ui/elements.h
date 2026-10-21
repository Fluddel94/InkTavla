#pragma once

// Building blocks shared by all widgets (style A of the overall UI design:
// white, hairlines, spaced capitals for tile titles).

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ui/canvas.h"
#include "ui/font.h"

namespace ui {

// --- Tiles ------------------------------------------------------------------

// Space between the edge of a slot and its content.
inline constexpr int kTilePadding = 14;

// Tile title: spaced capitals on the left, an optional note on the right
// (e.g. "MÜNCHEN", "KW 43"), a hairline below. Content starts at
// kTileTitleHeight.
inline constexpr int kTileTitleHeight = 33;

// Draws the tile title at the top of the canvas; both texts are shown in
// capitals and the title is shortened if it does not fit next to the note.
// With alert, the note is shown as it is in a black badge with a warning
// sign instead (age hint "Stand 14:32" for data that could not be updated).
void drawTileTitle(Canvas& canvas, std::string_view title, std::string_view note = {},
                   bool alert = false);

// Message in the content area of a tile (below the title), centred: warning
// sign, message and an optional line of detail. Used for widgets without
// data or settings.
void drawTileMessage(Canvas& canvas, std::string_view message, std::string_view detail = {});

// Section title inside a tile: small arrow, spaced capitals and a dotted
// line up to x1, kSectionTitleHeight high, starting at y.
inline constexpr int kSectionTitleHeight = 24;
void drawSectionTitle(Canvas& canvas, int x0, int x1, int y, std::string_view name);

// --- Symbols ----------------------------------------------------------------

// Warning sign: filled triangle with a white exclamation mark, standing on
// the baseline. Returns its width.
int drawWarningSign(Canvas& canvas, int x, int baseline, int height, Color color = Color::kBlack);

// Width of a warning sign of the given height.
int warningSignWidth(int height);

// Wi-Fi symbol (two arcs and a dot) in a size x size square at (x, y);
// struck through when not connected.
void drawWifiSymbol(Canvas& canvas, int x, int y, int size, bool connected = true);

// Draws a 1-bit XBM image (rows of (w + 7) / 8 bytes, least significant bit
// first, set bit = drawn), each pixel enlarged to scale x scale.
void drawXbm(Canvas& canvas, int x, int y, int w, int h, const uint8_t* bits, int scale = 1,
             Color color = Color::kBlack);

// Shapes of generated line symbols (work for any network).
enum class SymbolShape {
  kBox,         // filled box with slightly rounded corners, white label
  kRect,        // filled box with square corners (U-Bahn in the Briefing design)
  kPill,        // filled, fully rounded ends, white label
  kFrame,       // square frame, black label (e.g. regional trains)
  kRoundFrame,  // frame with rounded ends, black label (e.g. buses)
};

// Generated line symbol with the label centred in it.
void drawLineSymbol(Canvas& canvas, const Font& font, int x, int y, int width, int height,
                    std::string_view label, SymbolShape shape);

// --- Briefing ------------------------------------------------------------------
// Building blocks of the "Briefing" design (2026-10-21): columns without
// frames, titles in spaced capitals without a rule.

// Symbol in front of a column or section title.
enum class TitleIcon {
  kNone,
  kArrow,  // direction ("IN DIE STADT")
  kPin,    // station ("PASING")
};

// Column or section title with its baseline at `baseline`: optional icon,
// the title in spaced capitals (Bold 15, +2 px) from x0 and an optional note
// on the right ending at x1 (Medium 15, e.g. "KW 43", "4 min zu Fuß"). A
// note that would touch the title is left out. With alert the note is
// shown in a small black badge with a warning sign instead (age hint
// "Stand 14:32"); then the title is shortened if needed. Returns the x
// after the title.
int drawColumnTitle(Canvas& canvas, int x0, int x1, int baseline, std::string_view title,
                    std::string_view note = {}, TitleIcon icon = TitleIcon::kNone,
                    bool alert = false);

// One vehicle of a journey for drawLineSymbolChain().
struct ChainLink {
  std::string label;  // "S4", "RB55"
  SymbolShape shape = SymbolShape::kPill;
  int width = 0;         // symbol width; 0 = label width + 16
  std::string transfer;  // text under the gap after it, e.g. "12 min"
};

// Line symbols in a row, joined by a short line, for journeys with
// transfers; the transfer time is written small under each gap. centre_y is
// the vertical centre of the symbols. Returns the x after the last symbol.
// With draw = false only the width is measured.
inline constexpr int kChainGap = 16;
int drawLineSymbolChain(Canvas& canvas, const Font& font, const Font& small, int x, int centre_y,
                        int height, const std::vector<ChainLink>& links, bool draw = true);

}  // namespace ui
