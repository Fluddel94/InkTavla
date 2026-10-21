// Unit tests for the UI layer: bitmap, canvas, text, building blocks,
// layouts.

#include <unity.h>

#include <string>

#include "ui/bitmap.h"
#include "ui/elements.h"
#include "ui/fonts.h"
#include "ui/i18n.h"
#include "ui/layout.h"
#include "ui/text.h"

using ui::Bitmap;
using ui::BitmapCanvas;
using ui::Color;

namespace {

const ui::Font& font() { return ui::fonts::kRobotoCondensedMedium24; }

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
  TEST_ASSERT_TRUE(ui::textWidth(ui::fonts::kRobotoCondensedBold26, "München") >= a);
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
  ui::drawLineSymbol(c, f, 2, 3, 50, 26, "SEV", ui::SymbolShape::kRoundFrame);
  TEST_ASSERT_TRUE(b.get(2, 3) == Color::kWhite);  // rounded end
  TEST_ASSERT_TRUE(b.get(26, 3) == Color::kBlack);
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

void test_tile_title() {
  Bitmap b(358, 60);
  BitmapCanvas c(b);
  ui::drawTileTitle(c, "Wetter", "KW 43");
  // Hairline below the title from padding to padding.
  TEST_ASSERT_TRUE(b.get(ui::kTilePadding, 32) == Color::kBlack);
  TEST_ASSERT_TRUE(b.get(358 - ui::kTilePadding - 1, 32) == Color::kBlack);
  TEST_ASSERT_TRUE(b.get(ui::kTilePadding - 1, 32) == Color::kWhite);
  TEST_ASSERT_TRUE(b.get(100, 33) == Color::kWhite);
  const Box box = blackBox(b);
  TEST_ASSERT_INT_WITHIN(1, ui::kTilePadding, box.x0);
  TEST_ASSERT_INT_WITHIN(1, 358 - ui::kTilePadding - 1, box.x1);
  TEST_ASSERT_EQUAL_INT(24 - ui::fonts::kRobotoCondensedBold17.cap_height, box.y0);
  // A long title is shortened and never runs into the note.
  b.clear(Color::kWhite);
  ui::drawTileTitle(c, "Sehr lange Haltestellenbezeichnung am Ort", "Stand 07:31");
  bool gap = false;
  for (int x = 150; x < 344 && !gap; x++) {
    bool empty = true;
    for (int y = 0; y < 30; y++) empty = empty && b.get(x, y) == Color::kWhite;
    gap = empty;
  }
  TEST_ASSERT_TRUE(gap);
}

void test_section_title() {
  Bitmap b(358, 30);
  BitmapCanvas c(b);
  ui::drawSectionTitle(c, 14, 344, 0, "Stadt");
  TEST_ASSERT_TRUE(b.get(14, 11) == Color::kBlack);  // arrow shaft
  // Dotted line up to x1.
  TEST_ASSERT_TRUE(b.get(344, 11) == Color::kBlack || b.get(343, 11) == Color::kBlack);
  TEST_ASSERT_TRUE(b.get(344, 11) != b.get(343, 11));
  TEST_ASSERT_TRUE(b.get(345, 11) == Color::kWhite);
  TEST_ASSERT_TRUE(blackBox(b).y1 < ui::kSectionTitleHeight);
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

void test_layouts_cover_the_screen_without_overlap() {
  for (const char* name : {"one_plus_two", "three_columns"}) {
    const ui::Layout l = ui::layoutFor(name);
    TEST_ASSERT_EQUAL_size_t(3, l.slots.size());
    TEST_ASSERT_EQUAL_size_t(3, l.sizes.size());  // a size type for every slot
    TEST_ASSERT_TRUE(l.sizes[0] == ui::SlotSize::kColumn);
    TEST_ASSERT_EQUAL_INT(358, l.slots[0].w);
    Bitmap b(ui::kScreenWidth, ui::kScreenHeight);
    std::vector<ui::Rect> areas = l.slots;
    areas.push_back(l.header);
    areas.insert(areas.end(), l.separators.begin(), l.separators.end());
    int total = 0;
    for (const ui::Rect& r : areas) {
      for (int y = r.y; y < r.y + r.h; y++) {
        for (int x = r.x; x < r.x + r.w; x++) {
          TEST_ASSERT_TRUE_MESSAGE(b.get(x, y) == Color::kWhite, name);  // no overlap
          b.set(x, y, Color::kBlack);
          total++;
        }
      }
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(ui::kScreenWidth * ui::kScreenHeight, total, name);
  }
  TEST_ASSERT_EQUAL_INT(440, ui::layoutFor("one_plus_two").slots[1].w);
  TEST_ASSERT_EQUAL_INT(358, ui::layoutFor("unknown").slots[0].w);
}

void test_layout_slot_sizes() {
  using ui::SlotSize;
  const ui::Layout a = ui::layoutFor("one_plus_two");
  TEST_ASSERT_TRUE(a.sizes ==
                   (std::vector<SlotSize>{SlotSize::kColumn, SlotSize::kTile, SlotSize::kTile}));
  const ui::Layout b = ui::layoutFor("three_columns");
  TEST_ASSERT_TRUE(b.sizes == (std::vector<SlotSize>{SlotSize::kColumn, SlotSize::kNarrowColumn,
                                                     SlotSize::kNarrowColumn}));
  TEST_ASSERT_EQUAL_STRING("narrow column", ui::slotSizeName(SlotSize::kNarrowColumn));
}

int main() {
  UNITY_BEGIN();
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
  RUN_TEST(test_tile_title);
  RUN_TEST(test_section_title);
  RUN_TEST(test_wifi_symbol);
  RUN_TEST(test_i18n);
  RUN_TEST(test_layouts_cover_the_screen_without_overlap);
  RUN_TEST(test_layout_slot_sizes);
  return UNITY_END();
}
