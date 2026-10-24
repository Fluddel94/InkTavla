#pragma once

// Building blocks shared by all widgets ("Briefing" design, 2026-10-21):
// columns without frames, titles in spaced capitals without a rule.

#include <string>
#include <string_view>

#include "ui/canvas.h"
#include "ui/font.h"

namespace ui {

// --- Symbols ----------------------------------------------------------------

// Warning sign: filled triangle with a white exclamation mark, standing on
// the baseline. Returns its width.
int drawWarningSign(Canvas& canvas, int x, int baseline, int height, Color color = Color::kBlack);

// Width of a warning sign of the given height.
int warningSignWidth(int height);

// Wi-Fi symbol (two arcs and a dot) in a size x size square at (x, y);
// struck through when not connected.
void drawWifiSymbol(Canvas& canvas, int x, int y, int size, bool connected = true);

// Shapes of generated line symbols (work for any network).
enum class SymbolShape {
  kBox,    // filled box with slightly rounded corners, white label (tram)
  kRect,   // filled box with square corners, white label (U-Bahn)
  kPill,   // filled, fully rounded ends, white label (S-Bahn)
  kFrame,  // square frame, black label (buses, regional trains)
};

// Thickness of the frame of kFrame.
inline constexpr int kSymbolFrame = 2;

// Generated line symbol with the label centred in it.
void drawLineSymbol(Canvas& canvas, const Font& font, int x, int y, int width, int height,
                    std::string_view label, SymbolShape shape);

// --- Titles and messages ---------------------------------------------------------

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

// Baseline of the first column title (y 234 on the screen, columns start at
// y 205).
inline constexpr int kColumnTitleBaseline = 29;

// Message in a column (no data, settings missing), below the title: warning
// sign (unless `warning` is false, e.g. while loading), message (Medium 20)
// and detail (Medium 15), centred, both wrapped to the column width.
void drawColumnMessage(Canvas& canvas, std::string_view message, std::string_view detail = {},
                       bool warning = true);

}  // namespace ui
