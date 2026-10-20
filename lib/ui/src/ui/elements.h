#pragma once

// Building blocks shared by all widgets (style A of the overall UI design:
// white, hairlines, spaced capitals for tile titles).

#include <cstdint>
#include <string_view>

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
  kPill,        // filled, fully rounded ends, white label
  kFrame,       // square frame, black label (e.g. regional trains)
  kRoundFrame,  // frame with rounded ends, black label (e.g. buses)
};

// Generated line symbol with the label centred in it.
void drawLineSymbol(Canvas& canvas, const Font& font, int x, int y, int width, int height,
                    std::string_view label, SymbolShape shape);

}  // namespace ui
