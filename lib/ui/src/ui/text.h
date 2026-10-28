#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

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

// Text with extra letter spacing ("tracking"), used for the spaced capitals
// of column titles. spacing_x8 is added after every character but the last,
// in 1/8 pixel.
int trackedTextWidth(const Font& font, std::string_view text, int spacing_x8);
int drawTrackedText(Canvas& canvas, const Font& font, int x, int baseline, std::string_view text,
                    int spacing_x8, Color color = Color::kBlack, Align align = Align::kLeft);

// Upper case for the Latin characters of the fonts (ASCII, Latin-1, Latin
// Extended-A); "ß" becomes "SS". Other characters stay unchanged.
std::string toUpper(std::string_view text);

// Shortens a text to max_width by removing characters from the end and
// adding "." ("Petershausen" -> "Petersh."), without a space before the dot.
std::string fitText(const Font& font, std::string_view text, int max_width);

// Shortens several destinations "A/B/C" evenly: every part is cut to at
// most k characters (with "."), k decreases until everything fits; short
// parts stay complete as long as possible.
std::string fitEvenly(const Font& font, std::string_view text, int max_width);

// Shortens a text to max_width with "…" at the end ("Mittagessen mi…"), for
// titles of events and similar. Spaces and ",.-" before the ellipsis are
// removed. Unchanged if the text fits.
std::string fitTextEllipsis(const Font& font, std::string_view text, int max_width);

// Breaks a text into lines of at most max_width, between words (at
// spaces). A word longer than a line is shortened with "…". With max_lines
// > 0 at most that many lines are returned; if the text does not fit, the
// last line ends with "…" after a whole word, and *complete (if given) is
// set to false. With hyphenate, long German words are broken with a hyphen
// at the end of a line (see hyphen.h) instead of moving to the next line
// whole or, if they are wider than a line, being cut off.
std::vector<std::string> wrapText(const Font& font, std::string_view text, int max_width,
                                  int max_lines = 0, bool* complete = nullptr,
                                  bool hyphenate = false);

}  // namespace ui
