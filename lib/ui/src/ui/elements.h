#pragma once

// Building blocks shared by all widgets.

#include <cstdint>
#include <string_view>

#include "ui/canvas.h"
#include "ui/font.h"

namespace ui {

// Warning sign: filled triangle with a white exclamation mark, standing on
// the baseline. Returns its width.
int drawWarningSign(Canvas& canvas, int x, int baseline, int height, Color color = Color::kBlack);

// Width of a warning sign of the given height.
int warningSignWidth(int height);

// Draws a 1-bit XBM image (rows of (w + 7) / 8 bytes, least significant bit
// first, set bit = drawn), each pixel enlarged to scale x scale.
void drawXbm(Canvas& canvas, int x, int y, int w, int h, const uint8_t* bits, int scale = 1,
             Color color = Color::kBlack);

// Generated line symbol (works for any network): a filled box with the
// label in white, or a rounded frame with a black label (e.g. buses).
void drawLineSymbol(Canvas& canvas, const Font& font, int x, int y, int width, int height,
                    std::string_view label, bool frame);

}  // namespace ui
