#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ui/canvas.h"
#include "ui/font.h"

namespace ui {

// Next codepoint of a UTF-8 text starting at pos; pos moves to the next
// character. Invalid sequences give U+FFFD (drawn as '?') and skip one byte.
uint32_t decodeUtf8(std::string_view text, size_t& pos);

// Number of characters (codepoints).
size_t utf8Length(std::string_view text);

// Width of a text in pixels (sum of the advances).
int textWidth(const Font& font, std::string_view text);

enum class Align { kLeft, kCenter, kRight };

// Draws a UTF-8 text with its baseline at y. x is the left edge, the centre
// or the right edge depending on align. Returns the pen position after the
// text.
int drawText(Canvas& canvas, const Font& font, int x, int baseline, std::string_view text,
             Color color = Color::kBlack, Align align = Align::kLeft);

// Shortens a text to max_width by removing characters from the end and
// adding "." ("Petershausen" -> "Petersh."), without a space before the dot.
std::string fitText(const Font& font, std::string_view text, int max_width);

// Shortens several destinations "A/B/C" evenly: every part is cut to at
// most k characters (with "."), k decreases until everything fits; short
// parts stay complete as long as possible.
std::string fitEvenly(const Font& font, std::string_view text, int max_width);

}  // namespace ui
