// Unit tests for the UI layer: bitmap, canvas, text, building blocks,
// layouts.

#include <unity.h>

#include <string>
#include <vector>

#include "ui/bitmap.h"
#include "ui/elements.h"
#include "ui/emoji.h"
#include "ui/fonts.h"
#include "ui/hyphen.h"
#include "ui/i18n.h"
#include "ui/icons.h"
#include "ui/layout.h"
#include "ui/rows.h"
#include "ui/text.h"

using ui::Bitmap;
using ui::BitmapCanvas;
using ui::Color;

namespace {

const ui::Font& font() { return ui::fonts::kRobotoCondensedMedium20; }

int countBlack(const Bitmap& b) {
  int n = 0;
  for (int y = 0; y < b.height(); y++) {
    for (int x = 0; x < b.width(); x++) n += b.get(x, y) == Color::kBlack;
  }
  return n;
}

// Bounding box of all black pixels: {x0, y0, x1, y1}, x1/y1 inclusive.
struct Box {
  int x0 = 1 << 20, y0 = 1 << 20, x1 = -1, y1 = -1;
};
Box blackBox(const Bitmap& b) {
  Box box;
  for (int y = 0; y < b.height(); y++) {
    for (int x = 0; x < b.width(); x++) {
      if (b.get(x, y) != Color::kBlack) continue;
      box.x0 = std::min(box.x0, x);
      box.y0 = std::min(box.y0, y);
      box.x1 = std::max(box.x1, x);
      box.y1 = std::max(box.y1, y);
    }
  }
  return box;
}

}  // namespace

void setUp() {}
void tearDown() {}

// --- Bitmap and canvas -----------------------------------------------------

void test_bitmap_layout_msb_first_black_is_set() {
  Bitmap b(10, 2);
  TEST_ASSERT_EQUAL_size_t(2, b.stride());
  TEST_ASSERT_EQUAL_size_t(4, b.size());
  b.set(0, 0, Color::kBlack);
  b.set(9, 1, Color::kBlack);
  TEST_ASSERT_EQUAL_HEX8(0x80, b.data()[0]);
  TEST_ASSERT_EQUAL_HEX8(0x40, b.data()[3]);
  b.set(0, 0, Color::kWhite);
  TEST_ASSERT_EQUAL_HEX8(0x00, b.data()[0]);
  b.set(-1, 0, Color::kBlack);  // ignored
  b.set(10, 0, Color::kBlack);  // ignored
  TEST_ASSERT_EQUAL_INT(1, countBlack(b));
  b.clear(Color::kBlack);
  TEST_ASSERT_EQUAL_INT(20, countBlack(b));
}

void test_canvas_area_offset_and_clipping() {
  Bitmap b(20, 20);
  BitmapCanvas c(b, 5, 6, 4, 3);
  TEST_ASSERT_EQUAL_UINT16(4, c.width());
  TEST_ASSERT_EQUAL_UINT16(3, c.height());
  c.setPixel(0, 0, Color::kBlack);
  TEST_ASSERT_TRUE(b.get(5, 6) == Color::kBlack);
  c.fill(Color::kBlack);
  c.fillRect(-10, -10, 100, 100, Color::kBlack);
  c.drawLine(-5, -5, 30, 30, Color::kBlack);
  c.setPixel(4, 0, Color::kBlack);  // outside the area
  TEST_ASSERT_EQUAL_INT(12, countBlack(b));
  const Box box = blackBox(b);
  TEST_ASSERT_EQUAL_INT(5, box.x0);
  TEST_ASSERT_EQUAL_INT(6, box.y0);
  TEST_ASSERT_EQUAL_INT(8, box.x1);
  TEST_ASSERT_EQUAL_INT(8, box.y1);
}

void test_canvas_area_is_clipped_to_bitmap() {
  Bitmap b(10, 10);
  BitmapCanvas c(b, 8, -3, 5, 6);
  TEST_ASSERT_EQUAL_UINT16(2, c.width());
  TEST_ASSERT_EQUAL_UINT16(3, c.height());
}

void test_rect_outline_and_thickness() {
  Bitmap b(10, 10);
  BitmapCanvas c(b);
  c.drawRect(1, 1, 8, 6, Color::kBlack, 2);
  TEST_ASSERT_EQUAL_INT(8 * 6 - 4 * 2, countBlack(b));
  TEST_ASSERT_TRUE(b.get(4, 3) == Color::kWhite);
}

void test_line_end_points() {
  Bitmap b(10, 10);
  BitmapCanvas c(b);
  c.drawLine(1, 1, 8, 4, Color::kBlack);
  TEST_ASSERT_TRUE(b.get(1, 1) == Color::kBlack);
  TEST_ASSERT_TRUE(b.get(8, 4) == Color::kBlack);
  TEST_ASSERT_EQUAL_INT(8, countBlack(b));  // one pixel per column
}

void test_triangle_is_symmetric_and_inside() {
  Bitmap b(21, 12);
  BitmapCanvas c(b);
  c.fillTriangle(0, 10, 20, 10, 10, 0, Color::kBlack);
  const Box box = blackBox(b);
  TEST_ASSERT_EQUAL_INT(0, box.x0);
  TEST_ASSERT_EQUAL_INT(20, box.x1);
  TEST_ASSERT_EQUAL_INT(0, box.y0);
  TEST_ASSERT_EQUAL_INT(10, box.y1);
  for (int y = 0; y < 12; y++) {
    for (int x = 0; x < 21; x++) TEST_ASSERT_TRUE(b.get(x, y) == b.get(20 - x, y));
  }
}

void test_round_rect_stays_inside_and_cuts_corners() {
  Bitmap b(30, 20);
  BitmapCanvas c(b);
  c.fillRoundRect(2, 2, 26, 16, 6, Color::kBlack);
  const Box box = blackBox(b);
  TEST_ASSERT_EQUAL_INT(2, box.x0);
  TEST_ASSERT_EQUAL_INT(27, box.x1);
  TEST_ASSERT_EQUAL_INT(2, box.y0);
  TEST_ASSERT_EQUAL_INT(17, box.y1);
  TEST_ASSERT_TRUE(b.get(2, 2) == Color::kWhite);  // corner cut
  TEST_ASSERT_TRUE(b.get(15, 10) == Color::kBlack);

  Bitmap o(30, 20);
  BitmapCanvas co(o);
  co.drawRoundRect(2, 2, 26, 16, 6, Color::kBlack, 2);
  TEST_ASSERT_TRUE(o.get(15, 10) == Color::kWhite);  // hollow
  TEST_ASSERT_TRUE(o.get(15, 2) == Color::kBlack);
  TEST_ASSERT_TRUE(o.get(2, 10) == Color::kBlack);
  TEST_ASSERT_TRUE(o.get(2, 2) == Color::kWhite);
}

// --- Text -------------------------------------------------------------------

void test_utf8_decoding() {
  const std::string text = "aü€\xF0\x9F\x9A\x86";  // a, ü, €, train emoji
  size_t pos = 0;
  TEST_ASSERT_EQUAL_HEX32('a', ui::decodeUtf8(text, pos));
  TEST_ASSERT_EQUAL_HEX32(0xFC, ui::decodeUtf8(text, pos));
  TEST_ASSERT_EQUAL_HEX32(0x20AC, ui::decodeUtf8(text, pos));
  TEST_ASSERT_EQUAL_HEX32(0x1F686, ui::decodeUtf8(text, pos));
  TEST_ASSERT_EQUAL_size_t(text.size(), pos);
  TEST_ASSERT_EQUAL_size_t(4, ui::utf8Length(text));
}

void test_utf8_invalid_sequences() {
  const std::string text =
      "\xFC"
      "a\xC3";  // Latin-1 byte, a, truncated sequence
  size_t pos = 0;
  TEST_ASSERT_EQUAL_HEX32(0xFFFD, ui::decodeUtf8(text, pos));
  TEST_ASSERT_EQUAL_HEX32('a', ui::decodeUtf8(text, pos));
  TEST_ASSERT_EQUAL_HEX32(0xFFFD, ui::decodeUtf8(text, pos));
  TEST_ASSERT_EQUAL_size_t(3, pos);
}

void test_fonts_contain_german_characters() {
  for (uint32_t cp : {0xC4u, 0xD6u, 0xDCu, 0xE4u, 0xF6u, 0xFCu, 0xDFu, 0x20ACu, 0x2026u}) {
    TEST_ASSERT_EQUAL_HEX32(cp, ui::findGlyph(font(), cp).codepoint);
  }
  // Missing characters fall back to '?'.
  TEST_ASSERT_EQUAL_HEX32('?', ui::findGlyph(font(), 0x1F686).codepoint);
  TEST_ASSERT_EQUAL_HEX32('?', ui::findGlyph(font(), 0xFFFD).codepoint);
}

void test_text_width_adds_up() {
  TEST_ASSERT_EQUAL_INT(0, ui::textWidth(font(), ""));
  const int a = ui::textWidth(font(), "München");
  const int b = ui::textWidth(font(), " Hbf");
  const int ab = ui::textWidth(font(), "München Hbf");
  TEST_ASSERT_INT_WITHIN(1, a + b, ab);
  TEST_ASSERT_TRUE(ui::textWidth(ui::fonts::kRobotoCondensedBold21, "München") >= a);
}

void test_draw_text_position_and_alignment() {
  Bitmap b(200, 40);
  BitmapCanvas c(b);
  const int end = ui::drawText(c, font(), 10, 30, "Hbf");
  TEST_ASSERT_EQUAL_INT(10 + ui::textWidth(font(), "Hbf"), end);
  Box box = blackBox(b);
  TEST_ASSERT_INT_WITHIN(2, 10, box.x0);
  TEST_ASSERT_EQUAL_INT(29, box.y1);                  // stands on the baseline
  TEST_ASSERT_TRUE(box.y0 < 30 - font().cap_height);  // b and f are taller than H
  b.clear(Color::kWhite);
  ui::drawText(c, font(), 10, 30, "H");
  TEST_ASSERT_EQUAL_INT(30 - font().cap_height, blackBox(b).y0);
  b.clear(Color::kWhite);
  ui::drawText(c, font(), 190, 30, "Hbf", Color::kBlack, ui::Align::kRight);
  box = blackBox(b);
  TEST_ASSERT_INT_WITHIN(2, 189, box.x1);
}

void test_fit_text_like_the_departure_display() {
  const std::string full = "Petershausen";
  TEST_ASSERT_EQUAL_STRING("Petershausen",
                           ui::fitText(font(), full, ui::textWidth(font(), full)).c_str());
  const int max = ui::textWidth(font(), "Petersh.");
  TEST_ASSERT_EQUAL_STRING("Petersh.", ui::fitText(font(), full, max).c_str());
  TEST_ASSERT_EQUAL_STRING("Petersh.", ui::fitText(font(), full, max + 1).c_str());
  // No space before the dot.
  const int grafing = ui::textWidth(font(), "Grafing .");
  TEST_ASSERT_EQUAL_STRING("Grafing.", ui::fitText(font(), "Grafing Bahnhof", grafing).c_str());
  // Umlauts are cut as whole characters.
  const std::string cut = ui::fitText(font(), "Münchner Freiheit", ui::textWidth(font(), "Mü."));
  TEST_ASSERT_EQUAL_STRING("Mü.", cut.c_str());
}

void test_fit_evenly_cuts_long_parts_first() {
  const std::string text = "Bayrischzell/Lenggries/Tegernsee";
  const int max = ui::textWidth(font(), "Bayrisc./Lenggri./Tegerns.");
  TEST_ASSERT_EQUAL_STRING("Bayrisc./Lenggri./Tegerns.", ui::fitEvenly(font(), text, max).c_str());
  // Short parts stay complete.
  const std::string mixed = "Riem/Flughafen München";
  const int w = ui::textWidth(font(), "Riem/Flughaf.");
  TEST_ASSERT_EQUAL_STRING("Riem/Flughaf.", ui::fitEvenly(font(), mixed, w).c_str());
  TEST_ASSERT_EQUAL_STRING(text.c_str(),
                           ui::fitEvenly(font(), text, ui::textWidth(font(), text)).c_str());
}

// --- Building blocks, i18n, layout ----------------------------------------------

void test_warning_sign_size_and_mark() {
  Bitmap b(40, 30);
  BitmapCanvas c(b);
  const int w = ui::drawWarningSign(c, 5, 25, 19);
  TEST_ASSERT_EQUAL_INT(ui::warningSignWidth(19), w);
  const Box box = blackBox(b);
  TEST_ASSERT_EQUAL_INT(5, box.x0);
  TEST_ASSERT_EQUAL_INT(5 + w - 1, box.x1);
  TEST_ASSERT_EQUAL_INT(24, box.y1);
  TEST_ASSERT_EQUAL_INT(25 - 19, box.y0);
  TEST_ASSERT_TRUE(b.get(5 + (w - 1) / 2, 15) == Color::kWhite);  // exclamation mark
}

void test_line_symbol_box_and_frame() {
  Bitmap b(60, 40);
  BitmapCanvas c(b);
  const ui::Font& f = ui::fonts::kRobotoCondensedBold20;
  ui::drawLineSymbol(c, f, 2, 3, 50, 26, "RB6", ui::SymbolShape::kFrame);
  TEST_ASSERT_TRUE(b.get(2, 3) == Color::kBlack);  // square corner
  TEST_ASSERT_TRUE(b.get(51, 28) == Color::kBlack);
  TEST_ASSERT_TRUE(b.get(4, 16) == Color::kWhite);  // inside the 2 px frame
  b.clear(Color::kWhite);
  ui::drawLineSymbol(c, f, 2, 3, 50, 26, "S8", ui::SymbolShape::kPill);
  TEST_ASSERT_TRUE(b.get(2, 3) == Color::kWhite);
  TEST_ASSERT_TRUE(b.get(2, 16) == Color::kBlack);  // filled
  b.clear(Color::kWhite);
  ui::drawLineSymbol(c, f, 2, 3, 50, 26, "19", ui::SymbolShape::kBox);
  TEST_ASSERT_TRUE(b.get(2, 3) == Color::kWhite);  // slightly rounded corner
  TEST_ASSERT_TRUE(b.get(3, 3) == Color::kBlack);
  TEST_ASSERT_TRUE(b.get(2, 5) == Color::kBlack);
}

void test_tracked_text_adds_spacing() {
  const ui::Font& f = ui::fonts::kRobotoCondensedBold17;
  const int plain = ui::textWidth(f, "WETTER");
  TEST_ASSERT_INT_WITHIN(1, plain, ui::trackedTextWidth(f, "WETTER", 0));
  TEST_ASSERT_INT_WITHIN(1, plain + 10, ui::trackedTextWidth(f, "WETTER", 16));  // 5 gaps of 2 px
  TEST_ASSERT_EQUAL_INT(0, ui::trackedTextWidth(f, "", 16));
  Bitmap b(200, 30);
  BitmapCanvas c(b);
  const int end = ui::drawTrackedText(c, f, 190, 20, "KW 43", 12, Color::kBlack, ui::Align::kRight);
  TEST_ASSERT_INT_WITHIN(1, 190, end);
  TEST_ASSERT_INT_WITHIN(2, 189, blackBox(b).x1);
}

void test_upper_case() {
  TEST_ASSERT_EQUAL_STRING("DONNERSBERGERBRÜCKE", ui::toUpper("Donnersbergerbrücke").c_str());
  TEST_ASSERT_EQUAL_STRING("STRASSE", ui::toUpper("Straße").c_str());
  TEST_ASSERT_EQUAL_STRING("ÀÉÎÕØ ŁŚŻ KW 43", ui::toUpper("àéîõø łśż kw 43").c_str());
  TEST_ASSERT_EQUAL_STRING("÷ €", ui::toUpper("÷ €").c_str());
}

void test_polyline_is_exact_and_even() {
  Bitmap b(20, 12);
  BitmapCanvas c(b);
  // Horizontal at y = 5: the rows above and below the line, 2 px wide.
  c.drawPolyline({{2, 5}, {17, 5}}, 2, Color::kBlack);
  TEST_ASSERT_TRUE(b.get(8, 4) == Color::kBlack);
  TEST_ASSERT_TRUE(b.get(8, 5) == Color::kBlack);
  TEST_ASSERT_TRUE(b.get(8, 3) == Color::kWhite);
  TEST_ASSERT_TRUE(b.get(8, 6) == Color::kWhite);
  // Half a pixel lower moves the line by half: centred on row 5.
  b.clear(Color::kWhite);
  c.drawPolyline({{2, 5.5}, {17, 5.5}}, 2, Color::kBlack);
  TEST_ASSERT_TRUE(b.get(8, 4) == Color::kBlack && b.get(8, 5) == Color::kBlack &&
                   b.get(8, 6) == Color::kBlack);
  // A diagonal has the same width everywhere: 2 or 3 pixels per column.
  b.clear(Color::kWhite);
  c.drawPolyline({{1, 1}, {11, 11}}, 2, Color::kBlack);
  for (int x = 3; x < 10; x++) {
    int n = 0;
    for (int y = 0; y < 12; y++) n += b.get(x, y) == Color::kBlack;
    TEST_ASSERT_TRUE(n == 2 || n == 3);
  }
}

void test_wifi_symbol() {
  Bitmap b(30, 30);
  BitmapCanvas c(b);
  ui::drawWifiSymbol(c, 4, 4, 22);
  const Box box = blackBox(b);
  TEST_ASSERT_TRUE(box.x0 >= 4 && box.x1 < 26 && box.y0 >= 4 && box.y1 < 26);
  TEST_ASSERT_TRUE(b.get(4 + 11, 4 + 18) == Color::kBlack);  // dot
  // Symmetric left/right.
  for (int y = 0; y < 30; y++) {
    for (int x = 0; x < 11; x++) TEST_ASSERT_TRUE(b.get(4 + x, y) == b.get(4 + 21 - x, y));
  }
}

void test_i18n() {
  TEST_ASSERT_TRUE(ui::languageFromCode("en") == ui::Language::kEnglish);
  TEST_ASSERT_TRUE(ui::languageFromCode("de") == ui::Language::kGerman);
  TEST_ASSERT_TRUE(ui::languageFromCode("xx") == ui::Language::kGerman);
  TEST_ASSERT_EQUAL_STRING("jetzt", ui::text(ui::Language::kGerman, ui::Text::kNow));
  TEST_ASSERT_EQUAL_STRING("now", ui::text(ui::Language::kEnglish, ui::Text::kNow));
  TEST_ASSERT_EQUAL_STRING("Dienstag, 20. Oktober",
                           ui::longDate(ui::Language::kGerman, 2, 20, 10).c_str());
  TEST_ASSERT_EQUAL_STRING("Sunday, 1 March",
                           ui::longDate(ui::Language::kEnglish, 0, 1, 3).c_str());
  TEST_ASSERT_EQUAL_STRING("März", ui::monthName(ui::Language::kGerman, 3));
  TEST_ASSERT_EQUAL_STRING("", ui::monthName(ui::Language::kGerman, 13));
  TEST_ASSERT_EQUAL_STRING("", ui::weekdayName(ui::Language::kEnglish, 7));
}

void test_layout_covers_the_screen_without_overlap() {
  const ui::Layout l = ui::screenLayout();
  TEST_ASSERT_EQUAL_size_t(3, l.slots.size());
  TEST_ASSERT_EQUAL_size_t(3, l.steps.size());  // a size step for every column
  // Every layout: band + 3 with the wide column at each place, band + 2.
  std::vector<ui::LayoutOptions> all(4);
  all[1].wide_column = 1;
  all[2].wide_column = 2;
  all[3].columns = 2;
  for (const ui::LayoutOptions& options : all) {
    const ui::Layout layout = ui::screenLayout(options);
    TEST_ASSERT_EQUAL_size_t(static_cast<size_t>(options.columns), layout.slots.size());
    Bitmap b(ui::kScreenWidth, ui::kScreenHeight);
    std::vector<ui::Rect> areas = layout.slots;
    areas.push_back(layout.header);
    int total = 0;
    for (const ui::Rect& r : areas) {
      for (int y = r.y; y < r.y + r.h; y++) {
        for (int x = r.x; x < r.x + r.w; x++) {
          TEST_ASSERT_TRUE(b.get(x, y) == Color::kWhite);  // no overlap
          b.set(x, y, Color::kBlack);
          total++;
        }
      }
    }
    TEST_ASSERT_EQUAL_INT(ui::kScreenWidth * ui::kScreenHeight, total);
  }
}

void test_layout_columns_and_band() {
  const ui::Layout l = ui::screenLayout();
  TEST_ASSERT_EQUAL_INT(205, l.header.h);
  // Margin of 20 px at the sides, 15 px at the bottom. Content areas:
  // 20-300, 326-540, 566-780, from y 205 to 465; the slots split the gaps
  // (313, 553).
  TEST_ASSERT_EQUAL_INT(20, ui::kMargin);
  TEST_ASSERT_EQUAL_INT(15, ui::kMarginBottom);
  const int x[] = {20, 326, 566};
  const int w[] = {280, 214, 214};
  for (size_t i = 0; i < 3; i++) {
    const ui::Rect c = ui::contentOf(l, i);
    TEST_ASSERT_EQUAL_INT(x[i], c.x);
    TEST_ASSERT_EQUAL_INT(w[i], c.w);
    TEST_ASSERT_EQUAL_INT(205, c.y);
    TEST_ASSERT_EQUAL_INT(465, c.y + c.h);
    // Inside its slot.
    TEST_ASSERT_TRUE(c.x >= l.slots[i].x && c.x + c.w <= l.slots[i].x + l.slots[i].w);
  }
  using ui::SizeStep;
  TEST_ASSERT_TRUE(l.steps == (std::vector<SizeStep>{SizeStep::kM, SizeStep::kS, SizeStep::kS}));
  TEST_ASSERT_EQUAL_INT(214, ui::stepWidth(SizeStep::kS));
  TEST_ASSERT_EQUAL_INT(280, ui::stepWidth(SizeStep::kM));
  TEST_ASSERT_EQUAL_INT(367, ui::stepWidth(SizeStep::kL));
  TEST_ASSERT_EQUAL_STRING("M", ui::sizeStepName(SizeStep::kM));
  TEST_ASSERT_EQUAL_INT(313, l.slots[1].x);
  TEST_ASSERT_EQUAL_INT(553, l.slots[2].x);
  TEST_ASSERT_EQUAL_INT(800, l.slots[2].x + l.slots[2].w);
  // Dotted lines between the columns by default, in the middle of the gaps.
  TEST_ASSERT_EQUAL_size_t(2, l.dividers.size());
  TEST_ASSERT_EQUAL_INT(313, l.dividers[0].x);
  TEST_ASSERT_EQUAL_INT(553, l.dividers[1].x);
  TEST_ASSERT_EQUAL_INT(ui::kDividerTop, l.dividers[0].y);
  TEST_ASSERT_EQUAL_INT(465, l.dividers[0].y + l.dividers[0].h);
  // The header line with the clock is its own refresh area.
  TEST_ASSERT_EQUAL_size_t(3, l.header_areas.size());
  TEST_ASSERT_EQUAL_INT(44, l.header_areas[0].h);
  int area = 0;
  for (const ui::Rect& r : l.header_areas) area += r.w * r.h;
  TEST_ASSERT_EQUAL_INT(800 * 205, area);
}

// Content areas (x from-to) and dividers of a layout.
void checkColumns(const ui::LayoutOptions& options, const std::vector<int>& from,
                  const std::vector<int>& to, const std::vector<int>& lines) {
  const ui::Layout l = ui::screenLayout(options);
  TEST_ASSERT_EQUAL_size_t(from.size(), l.content.size());
  TEST_ASSERT_EQUAL_size_t(from.size(), l.steps.size());
  for (size_t i = 0; i < from.size(); i++) {
    const ui::Rect c = ui::contentOf(l, i);
    TEST_ASSERT_EQUAL_INT(from[i], c.x);
    TEST_ASSERT_EQUAL_INT(to[i], c.x + c.w);
    TEST_ASSERT_EQUAL_INT(ui::stepWidth(l.steps[i]), c.w);
    TEST_ASSERT_EQUAL_INT(205, c.y);
    TEST_ASSERT_EQUAL_INT(465, c.y + c.h);
    TEST_ASSERT_TRUE(c.x >= l.slots[i].x && c.x + c.w <= l.slots[i].x + l.slots[i].w);
  }
  // The dotted lines in the middle of the gaps, at the left edge of a slot.
  TEST_ASSERT_EQUAL_size_t(lines.size(), l.dividers.size());
  for (size_t i = 0; i < lines.size(); i++) {
    TEST_ASSERT_EQUAL_INT(lines[i], l.dividers[i].x);
    TEST_ASSERT_EQUAL_INT(l.slots[i + 1].x, l.dividers[i].x);
    TEST_ASSERT_EQUAL_INT(465, l.dividers[i].y + l.dividers[i].h);
  }
}

void test_layout_wide_column_moves_and_band_plus_two() {
  ui::LayoutOptions options;
  checkColumns(options, {20, 326, 566}, {300, 540, 780}, {313, 553});
  options.wide_column = 1;
  checkColumns(options, {20, 260, 566}, {234, 540, 780}, {247, 553});
  options.wide_column = 2;
  checkColumns(options, {20, 260, 500}, {234, 474, 780}, {247, 487});
  options.wide_column = 7;  // out of range: the first column
  checkColumns(options, {20, 326, 566}, {300, 540, 780}, {313, 553});
  // Band + 2: two columns of 367 px, the slots are the halves of the screen.
  options = {};
  options.columns = 2;
  checkColumns(options, {20, 413}, {387, 780}, {400});
  const ui::Layout two = ui::screenLayout(options);
  TEST_ASSERT_TRUE(two.steps == (std::vector<ui::SizeStep>{ui::SizeStep::kL, ui::SizeStep::kL}));
  TEST_ASSERT_EQUAL_INT(400, two.slots[0].w);
  TEST_ASSERT_EQUAL_INT(400, two.slots[1].w);
  // Without lines ("dividers": false): none.
  options.dividers = false;
  TEST_ASSERT_EQUAL_size_t(0, ui::screenLayout(options).dividers.size());
}

// --- Briefing building blocks ---------------------------------------------------

void test_briefing_fonts_contain_the_needed_characters() {
  const ui::Font* fonts[] = {&ui::fonts::kFraunces38,
                             &ui::fonts::kFraunces34,
                             &ui::fonts::kFraunces24Medium,
                             &ui::fonts::kFraunces20,
                             &ui::fonts::kFraunces20Bold,
                             &ui::fonts::kFraunces18,
                             &ui::fonts::kRobotoCondensedBold15,
                             &ui::fonts::kRobotoCondensedMedium13,
                             &ui::fonts::kRobotoCondensedMedium15,
                             &ui::fonts::kRobotoCondensedMedium16};
  for (const ui::Font* f : fonts) {
    for (uint32_t cp :
         {0xC4u, 0xD6u, 0xDCu, 0xE4u, 0xF6u, 0xFCu, 0xDFu, 0xB0u, 0x2013u, 0x2026u, 0xB7u}) {
      TEST_ASSERT_EQUAL_HEX32_MESSAGE(cp, ui::findGlyph(*f, cp).codepoint, f->name);
    }
  }
  TEST_ASSERT_EQUAL_STRING("Fraunces 420 38", ui::fonts::kFraunces38.name);
  TEST_ASSERT_EQUAL_INT(38, ui::fonts::kFraunces38.size);
  // The bold variant is wider.
  TEST_ASSERT_TRUE(ui::textWidth(ui::fonts::kFraunces20Bold, "Team-Standup") >
                   ui::textWidth(ui::fonts::kFraunces20, "Team-Standup"));
}

void test_fit_text_with_ellipsis() {
  const ui::Font& f = ui::fonts::kFraunces20;
  TEST_ASSERT_EQUAL_STRING("Zahnarzt", ui::fitTextEllipsis(f, "Zahnarzt", 200).c_str());
  const std::string s = ui::fitTextEllipsis(f, "Mittagessen mit Jonas", 164);
  TEST_ASSERT_TRUE(ui::textWidth(f, s) <= 164);
  TEST_ASSERT_TRUE(s.size() > 3 && s.substr(s.size() - 3) == "\u2026");
  TEST_ASSERT_TRUE(s.rfind("Mittagessen", 0) == 0);
  // No space or comma before the ellipsis.
  const std::string t = ui::fitTextEllipsis(f, "Kino, danach Essen", ui::textWidth(f, "Kino,X"));
  TEST_ASSERT_EQUAL_STRING("Kino\u2026", t.c_str());
  // Nothing fits: only the ellipsis.
  TEST_ASSERT_EQUAL_STRING("\u2026", ui::fitTextEllipsis(f, "Zahnarzt", 3).c_str());
}

void test_wrap_text_between_words() {
  const ui::Font& f = ui::fonts::kFraunces38;
  const char* sentence =
      "Guten Morgen. Bis Mittag bleibt es trocken, ab 14 Uhr regnet es \u2013 Schirm mit.";
  bool complete = false;
  const std::vector<std::string> lines = ui::wrapText(f, sentence, 564, 3, &complete);
  TEST_ASSERT_TRUE(complete);
  TEST_ASSERT_EQUAL_size_t(3, lines.size());
  std::string joined;
  for (const std::string& l : lines) {
    TEST_ASSERT_TRUE(ui::textWidth(f, l) <= 564);
    TEST_ASSERT_TRUE(l.front() != ' ' && l.back() != ' ');
    joined += (joined.empty() ? "" : " ") + l;
  }
  TEST_ASSERT_EQUAL_STRING(sentence, joined.c_str());
  TEST_ASSERT_EQUAL_STRING("Guten Morgen. Bis Mittag bleibt", lines[0].c_str());

  // Too long for two lines: the second one ends with "…" after a word.
  const std::vector<std::string> two = ui::wrapText(f, sentence, 564, 2, &complete);
  TEST_ASSERT_FALSE(complete);
  TEST_ASSERT_EQUAL_size_t(2, two.size());
  TEST_ASSERT_TRUE(two[1].substr(two[1].size() - 3) == "\u2026");
  TEST_ASSERT_TRUE(ui::textWidth(f, two[1]) <= 564);
  TEST_ASSERT_TRUE(two[1].find(' ') != std::string::npos);

  // Without a limit everything is returned; a word longer than a line is
  // shortened.
  const std::vector<std::string> narrow =
      ui::wrapText(ui::fonts::kRobotoCondensedMedium17, "Signalstörung Donnersbergerbrücke", 90);
  TEST_ASSERT_EQUAL_size_t(2, narrow.size());
  TEST_ASSERT_TRUE(ui::textWidth(ui::fonts::kRobotoCondensedMedium17, narrow[1]) <= 90);
  TEST_ASSERT_EQUAL_size_t(0, ui::wrapText(f, "   ", 100).size());
}

void test_fill_rule_rows() {
  // Briefing departures: 230 px below the first title, a second section
  // title 28 px high, rows of 28 px -> 7 rows (4 + 3).
  TEST_ASSERT_EQUAL_INT(7, ui::rowsThatFit(465 - 240, 28, 2, 28));
  TEST_ASSERT_EQUAL_INT(8, ui::rowsThatFit(465 - 240, 28));
  TEST_ASSERT_EQUAL_INT(0, ui::rowsThatFit(20, 28));
  TEST_ASSERT_EQUAL_INT(0, ui::rowsThatFit(100, 0));
  using V = std::vector<int>;
  TEST_ASSERT_TRUE(ui::distributeRows(7, V{9, 9}) == (V{4, 3}));
  TEST_ASSERT_TRUE(ui::distributeRows(7, V{2, 9}) == (V{2, 5}));
  TEST_ASSERT_TRUE(ui::distributeRows(7, V{0, 9}) == (V{1, 6}));
}

void test_dotted_fill_every_second_pixel() {
  Bitmap b(10, 10);
  BitmapCanvas c(b);
  c.fillDotted(1, 1, 6, 5);
  // Pixels at even coordinates inside x 1..6, y 1..5.
  TEST_ASSERT_EQUAL_INT(3 * 2, countBlack(b));
  TEST_ASSERT_TRUE(b.get(2, 2) == Color::kBlack);
  TEST_ASSERT_TRUE(b.get(6, 4) == Color::kBlack);
  TEST_ASSERT_TRUE(b.get(1, 1) == Color::kWhite);
  TEST_ASSERT_TRUE(b.get(3, 2) == Color::kWhite);
}

void test_column_title_note_and_badge() {
  Bitmap b(214, 30);
  BitmapCanvas c(b);
  const int end = ui::drawColumnTitle(c, 0, 214, 20, "Heute", "KW 43");
  TEST_ASSERT_TRUE(end > 40 && end < 100);
  Box box = blackBox(b);
  TEST_ASSERT_EQUAL_INT(213, box.x1);  // the note ends at the right edge
  // A note that would touch the title is left out.
  Bitmap b2(170, 30);
  BitmapCanvas c2(b2);
  const int end2 =
      ui::drawColumnTitle(c2, 0, 170, 20, "In die Stadt", "4 min zu Fuß", ui::TitleIcon::kArrow);
  TEST_ASSERT_TRUE(blackBox(b2).x1 <= end2);
  // A title wider than the column is shortened.
  Bitmap b4(80, 30);
  BitmapCanvas c4(b4);
  TEST_ASSERT_TRUE(ui::drawColumnTitle(c4, 0, 80, 20, "Pasing Marienplatz") <= 80);
  // Badge: black area at the right, the title shortened.
  Bitmap b3(214, 30);
  BitmapCanvas c3(b3);
  ui::drawColumnTitle(c3, 0, 214, 20, "Abfahrten Pasing", "Stand 07:41", ui::TitleIcon::kPin, true);
  TEST_ASSERT_TRUE(b3.get(212, 12) == Color::kBlack);
  TEST_ASSERT_TRUE(b3.get(3, 10) == Color::kBlack);  // pin
  // Stop sign: ring on the left edge, hollow between ring and H.
  Bitmap b5(214, 30);
  BitmapCanvas c5(b5);
  ui::drawColumnTitle(c5, 0, 214, 20, "Pasing", "", ui::TitleIcon::kStation);
  TEST_ASSERT_TRUE(b5.get(1, 10) == Color::kBlack);
  TEST_ASSERT_TRUE(b5.get(3, 10) == Color::kWhite);
}

void test_briefing_icons() {
  for (ui::WeatherIcon kind : {ui::WeatherIcon::kSun, ui::WeatherIcon::kRain,
                               ui::WeatherIcon::kSnow, ui::WeatherIcon::kMoon}) {
    const ui::Icon& band = ui::bandIcon(kind);
    TEST_ASSERT_EQUAL_INT(ui::kBandIconWidth, band.width);
    TEST_ASSERT_EQUAL_INT(ui::kBandIconHeight, band.height);
    TEST_ASSERT_EQUAL_INT(44, ui::weatherIcon(kind, 44).width);
    TEST_ASSERT_EQUAL_INT(30, ui::weatherIcon(kind, 30).width);  // parts on step L (V1)
  }
  TEST_ASSERT_TRUE(ui::bandIcon(ui::WeatherIcon::kSun).bits !=
                   ui::bandIcon(ui::WeatherIcon::kRain).bits);
  TEST_ASSERT_EQUAL_INT(44, ui::weatherIcon(ui::WeatherIcon::kSun, 40).width);  // nearest size
  TEST_ASSERT_EQUAL_INT(30, ui::weatherIcon(ui::WeatherIcon::kSun, 20).width);
  TEST_ASSERT_EQUAL_INT(15, ui::kCalendarIcon.width);
  TEST_ASSERT_EQUAL_INT(16, ui::kPinIcon.height);
  TEST_ASSERT_EQUAL_INT(16, ui::kStationIcon.width);
  TEST_ASSERT_EQUAL_INT(16, ui::kStationIcon.height);
}

// The word with "|" at every break the hyphenator allows (a hyphen that the
// word has already counts as a break after it).
std::string hyphenated(const std::string& word) {
  std::string result;
  size_t last = 0;
  for (const ui::HyphenPoint& p : ui::hyphenationPoints(word)) {
    result += word.substr(last, p.offset - last) + "|";
    last = p.offset;
  }
  return result + word.substr(last);
}

void test_hyphenation_of_railway_words() {
  // Compound boundaries first, then syllables; a linking "s" stays in front.
  TEST_ASSERT_EQUAL_STRING("Stell|werks|stö|rung", hyphenated("Stellwerksstörung").c_str());
  TEST_ASSERT_EQUAL_STRING("Ver|spä|tung", hyphenated("Verspätung").c_str());
  TEST_ASSERT_EQUAL_STRING("Bahn|über|gang", hyphenated("Bahnübergang").c_str());
  TEST_ASSERT_EQUAL_STRING("Zug|aus|fall", hyphenated("Zugausfall").c_str());
  // The "s" is a linking "s" only if a known stem follows it (CR2b M5; was
  // "Signals|törung", "Unfalls|telle", "Bahnhofs|traße", "Weiche|nstörung").
  TEST_ASSERT_EQUAL_STRING("Sig|nal|stö|rung", hyphenated("Signalstörung").c_str());
  TEST_ASSERT_EQUAL_STRING("Un|fall|stelle", hyphenated("Unfallstelle").c_str());
  TEST_ASSERT_EQUAL_STRING("Bahn|hof|straße", hyphenated("Bahnhofstraße").c_str());
  TEST_ASSERT_EQUAL_STRING("Wei|chen|stö|rung", hyphenated("Weichenstörung").c_str());
  TEST_ASSERT_EQUAL_STRING("Stell|werks|stö|rung", hyphenated("Stellwerksstörung").c_str());
  // Existing hyphens are kept (no second one is added).
  TEST_ASSERT_EQUAL_STRING("S-|Bahn-|Stre|cke", hyphenated("S-Bahn-Strecke").c_str());
  // Words not in the dictionary: syllable rules ("ck" and "ch" stay together).
  TEST_ASSERT_EQUAL_STRING("Fens|ter", hyphenated("Fenster").c_str());
  TEST_ASSERT_EQUAL_STRING("Zu|cker", hyphenated("Zucker").c_str());
  // "schr", "spr" and "str" start a syllable together (2026-11-05, was
  // "Besp|re|chung").
  TEST_ASSERT_EQUAL_STRING("Be|spre|chung", hyphenated("Besprechung").c_str());
  TEST_ASSERT_EQUAL_STRING("be|schrei|ben", hyphenated("beschreiben").c_str());
  TEST_ASSERT_EQUAL_STRING("ab|strakt", hyphenated("abstrakt").c_str());
  // Short words and numbers stay whole: two letters before, three after.
  TEST_ASSERT_EQUAL_STRING("Hbf", hyphenated("Hbf").c_str());
  TEST_ASSERT_EQUAL_STRING("Mün|chen", hyphenated("München").c_str());
  TEST_ASSERT_EQUAL_STRING("2026", hyphenated("2026").c_str());
  TEST_ASSERT_EQUAL_STRING("S2", hyphenated("S2").c_str());
  // The hyphen is added only where the word has none.
  const auto points = ui::hyphenationPoints("S-Bahn-Strecke");
  TEST_ASSERT_TRUE(points[0].at_hyphen);
}

void test_wrap_with_hyphenation_does_not_cut_words() {
  const ui::Font& f = ui::fonts::kRobotoCondensedMedium17;
  const std::string text = "Signalstörung Donnersbergerbrücke";
  // Without hyphenation the long word is cut off with "…".
  const auto cut = ui::wrapText(f, text, 90, 0, nullptr, false);
  TEST_ASSERT_TRUE(cut.back().find("\xE2\x80\xA6") != std::string::npos);
  // With it nothing is lost: the lines joined (hyphens removed) are the text.
  bool complete = false;
  const auto lines = ui::wrapText(f, text, 90, 0, &complete, true);
  TEST_ASSERT_TRUE(complete);
  std::string joined;
  bool hyphenated_end = false;
  for (const std::string& line : lines) {
    TEST_ASSERT_TRUE(ui::textWidth(f, line) <= 90);
    if (!joined.empty() && !hyphenated_end) joined += " ";
    hyphenated_end = line.back() == '-';
    joined += hyphenated_end ? line.substr(0, line.size() - 1) : line;
  }
  TEST_ASSERT_EQUAL_STRING(text.c_str(), joined.c_str());
  TEST_ASSERT_TRUE(lines.size() >= 3);
  // A short text is wrapped as before.
  const auto same = ui::wrapText(f, "Bauarbeiten am Bahnhof", 256, 0, nullptr, true);
  TEST_ASSERT_EQUAL_size_t(1, same.size());
}

// The last line may not be hyphenated into a line that is not there: with
// max_lines the text ends with "…" after a whole word or part.
void test_wrap_with_hyphenation_respects_max_lines() {
  const ui::Font& f = ui::fonts::kRobotoCondensedMedium17;
  bool complete = true;
  const auto lines = ui::wrapText(
      f,
      "Wegen einer Stellwerksstörung kommt es zu Verspätungen und Ausfällen zwischen Dachau "
      "und Petershausen",
      200, 2, &complete, true);
  TEST_ASSERT_EQUAL_size_t(2, lines.size());
  TEST_ASSERT_FALSE(complete);
  for (const std::string& line : lines) TEST_ASSERT_TRUE(ui::textWidth(f, line) <= 200);
}

// The last line of a text that does not fit can end at the last letter that
// fits instead of after a whole word (2026-11-23, disruption text).
void test_wrap_cut_at_letter() {
  const ui::Font& f = ui::fonts::kRobotoCondensedMedium17;
  const std::string text =
      "Wegen einer Stellwerksstörung kommt es zu Verspätungen und Ausfällen auf der gesamten "
      "Strecke zwischen Petershausen und München Hbf";
  bool complete = true;
  // Default: after a whole word.
  auto lines = ui::wrapText(f, text, 256, 1, &complete, true);
  TEST_ASSERT_FALSE(complete);
  TEST_ASSERT_EQUAL_size_t(1, lines.size());
  TEST_ASSERT_EQUAL_STRING("Wegen einer Stellwerksstörung\u2026", lines[0].c_str());
  // One line, cut at the letter.
  complete = true;
  lines = ui::wrapText(f, text, 256, 1, &complete, true, true);
  TEST_ASSERT_FALSE(complete);
  TEST_ASSERT_EQUAL_size_t(1, lines.size());
  TEST_ASSERT_EQUAL_STRING("Wegen einer Stellwerksstörung kom\u2026", lines[0].c_str());
  // Two lines: only the last one is cut, the first one is as before.
  lines = ui::wrapText(f, text, 256, 2, &complete, true, true);
  TEST_ASSERT_EQUAL_size_t(2, lines.size());
  TEST_ASSERT_EQUAL_STRING("Wegen einer Stellwerksstörung", lines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("kommt es zu Verspätungen und Aus\u2026", lines[1].c_str());
  // A text that fits is not changed, whatever the parameter.
  complete = false;
  lines = ui::wrapText(f, "Bauarbeiten am Bahnhof", 256, 1, &complete, true, true);
  TEST_ASSERT_TRUE(complete);
  TEST_ASSERT_EQUAL_STRING("Bauarbeiten am Bahnhof", lines[0].c_str());
  // Any width: the line fits, ends with "…" and has no broken character.
  for (int width = 60; width <= 256; width += 7) {
    for (const std::string& line : ui::wrapText(f, text, width, 2, &complete, true, true)) {
      TEST_ASSERT_TRUE(ui::textWidth(f, line) <= width);
      for (size_t pos = 0; pos < line.size();)
        TEST_ASSERT_TRUE(ui::decodeUtf8(line, pos) != 0xFFFD);
    }
    TEST_ASSERT_FALSE(complete);
  }
}

// Codepoint of an emoji icon as UTF-8 (private use area, three bytes).
std::string emojiUtf8(ui::EmojiIcon icon) {
  const uint32_t cp = ui::emojiCodepoint(icon);
  std::string out;
  out += static_cast<char>(0xE0 | (cp >> 12));
  out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
  out += static_cast<char>(0x80 | (cp & 0x3F));
  return out;
}

void test_emoji_mapping() {
  using ui::EmojiIcon;
  // Text without emoji stays as it is, also with umlauts and double spaces.
  TEST_ASSERT_EQUAL_STRING(
      "Gr\xC3\xBC\xC3\x9F"
      "e  aus M\xC3\xBCnchen",
      ui::mapEmoji("Gr\xC3\xBC\xC3\x9F"
                   "e  aus M\xC3\xBCnchen")
          .c_str());
  // All hearts, with and without the colour selector, are one icon.
  const std::string heart = emojiUtf8(EmojiIcon::kHeart);
  TEST_ASSERT_EQUAL_STRING(heart.c_str(), ui::mapEmoji("\xE2\x9D\xA4").c_str());
  TEST_ASSERT_EQUAL_STRING(heart.c_str(), ui::mapEmoji("\xE2\x9D\xA4\xEF\xB8\x8F").c_str());
  TEST_ASSERT_EQUAL_STRING(heart.c_str(), ui::mapEmoji("\xF0\x9F\x92\x99").c_str());  // blue
  // Skin tone, zero width joiner, flags and unknown emoji disappear; the
  // spaces around them are merged and trimmed.
  TEST_ASSERT_EQUAL_STRING((emojiUtf8(EmojiIcon::kThumbsUp)).c_str(),
                           ui::mapEmoji("\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD").c_str());
  TEST_ASSERT_EQUAL_STRING("a b", ui::mapEmoji("a \xF0\x9F\xA6\x84 b").c_str());
  TEST_ASSERT_EQUAL_STRING(
      "Rom", ui::mapEmoji("\xF0\x9F\xA6\x84 Rom \xF0\x9F\x87\xAE\xF0\x9F\x87\xB9").c_str());
  TEST_ASSERT_EQUAL_STRING("", ui::mapEmoji("\xF0\x9F\xA6\x84").c_str());
  // Clock faces share the clock icon.
  TEST_ASSERT_EQUAL_STRING(emojiUtf8(EmojiIcon::kClock).c_str(),
                           ui::mapEmoji("\xF0\x9F\x95\x93").c_str());
}

// Every text font has a glyph for each icon (not the "?" replacement).
void test_emoji_icons_are_in_all_fonts() {
  const ui::Font* fonts[] = {&ui::fonts::kRobotoCondensedBold15,
                             &ui::fonts::kRobotoCondensedBold16,
                             &ui::fonts::kRobotoCondensedBold17,
                             &ui::fonts::kRobotoCondensedBold20,
                             &ui::fonts::kRobotoCondensedBold21,
                             &ui::fonts::kRobotoCondensedMedium13,
                             &ui::fonts::kRobotoCondensedMedium15,
                             &ui::fonts::kRobotoCondensedMedium16,
                             &ui::fonts::kRobotoCondensedMedium17,
                             &ui::fonts::kRobotoCondensedMedium20,
                             &ui::fonts::kFraunces38,
                             &ui::fonts::kFraunces34,
                             &ui::fonts::kFraunces24Medium,
                             &ui::fonts::kFraunces20,
                             &ui::fonts::kFraunces20Bold,
                             &ui::fonts::kFraunces18};
  for (const ui::Font* f : fonts) {
    for (size_t i = 0; i < ui::kEmojiIconCount; i++) {
      const uint32_t cp = ui::kEmojiFirstCodepoint + static_cast<uint32_t>(i);
      TEST_ASSERT_EQUAL_UINT32_MESSAGE(cp, ui::findGlyph(*f, cp).codepoint, f->name);
      TEST_ASSERT_TRUE_MESSAGE(ui::findGlyph(*f, cp).width > 0, f->name);
    }
  }
  // An icon is about as wide as it is high and takes part in the width.
  const std::string text = "A" + emojiUtf8(ui::EmojiIcon::kHeart);
  TEST_ASSERT_TRUE(ui::textWidth(ui::fonts::kFraunces20, text) >
                   ui::textWidth(ui::fonts::kFraunces20, "A") + 15);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_hyphenation_of_railway_words);
  RUN_TEST(test_wrap_with_hyphenation_does_not_cut_words);
  RUN_TEST(test_wrap_with_hyphenation_respects_max_lines);
  RUN_TEST(test_wrap_cut_at_letter);
  RUN_TEST(test_bitmap_layout_msb_first_black_is_set);
  RUN_TEST(test_canvas_area_offset_and_clipping);
  RUN_TEST(test_canvas_area_is_clipped_to_bitmap);
  RUN_TEST(test_rect_outline_and_thickness);
  RUN_TEST(test_line_end_points);
  RUN_TEST(test_triangle_is_symmetric_and_inside);
  RUN_TEST(test_round_rect_stays_inside_and_cuts_corners);
  RUN_TEST(test_utf8_decoding);
  RUN_TEST(test_utf8_invalid_sequences);
  RUN_TEST(test_fonts_contain_german_characters);
  RUN_TEST(test_text_width_adds_up);
  RUN_TEST(test_draw_text_position_and_alignment);
  RUN_TEST(test_fit_text_like_the_departure_display);
  RUN_TEST(test_fit_evenly_cuts_long_parts_first);
  RUN_TEST(test_warning_sign_size_and_mark);
  RUN_TEST(test_line_symbol_box_and_frame);
  RUN_TEST(test_tracked_text_adds_spacing);
  RUN_TEST(test_upper_case);
  RUN_TEST(test_polyline_is_exact_and_even);
  RUN_TEST(test_wifi_symbol);
  RUN_TEST(test_i18n);
  RUN_TEST(test_layout_covers_the_screen_without_overlap);
  RUN_TEST(test_layout_columns_and_band);
  RUN_TEST(test_layout_wide_column_moves_and_band_plus_two);
  RUN_TEST(test_briefing_fonts_contain_the_needed_characters);
  RUN_TEST(test_fit_text_with_ellipsis);
  RUN_TEST(test_wrap_text_between_words);
  RUN_TEST(test_fill_rule_rows);
  RUN_TEST(test_dotted_fill_every_second_pixel);
  RUN_TEST(test_column_title_note_and_badge);
  RUN_TEST(test_briefing_icons);
  RUN_TEST(test_emoji_mapping);
  RUN_TEST(test_emoji_icons_are_in_all_fonts);
  return UNITY_END();
}
