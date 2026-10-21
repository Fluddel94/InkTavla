// Screen previews drawn with the real UI and widget code.
//
// Each test renders a screen from real MVG data, writes it as PNG to
// .pio/preview/ and compares it with the reference image in
// test/fixtures/ui/. After an intended change of the appearance, update the
// references with:
//
//   INKTAVLA_UPDATE_PREVIEWS=1 pio test -e native -f test_ui_preview

#include <unity.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/time.h"
#include "hal/png.h"
#include "ui/bitmap.h"
#include "ui/elements.h"
#include "ui/fonts.h"
#include "ui/i18n.h"
#include "ui/icons.h"
#include "ui/layout.h"
#include "ui/text.h"
#include "widgets/header/header.h"
#include "widgets/mvg/config.h"
#include "widgets/mvg/departures.h"
#include "widgets/mvg/sections.h"
#include "widgets/mvg/view.h"

namespace {

using widgets::mvg::Section;
using widgets::mvg::TimeFormat;

constexpr int64_t kNow = 1792441500;  // 2026-10-19 22:25 CEST

JsonDocument load(const char* fixture) {
  std::ifstream file(std::string("test/fixtures/mvg/") + fixture, std::ios::binary);
  JsonDocument doc;
  widgets::mvg::readDepartures(file, doc);
  return doc;
}

// All departures of a fixture as one list.
std::vector<Section> list(const char* fixture) {
  widgets::mvg::LineSelection all;
  return {{"", widgets::mvg::parseDepartures(load(fixture), widgets::mvg::DirectionFilter::kAll,
                                             all, 20)}};
}

// Pasing in two sections as configured in the example file (the tram
// lines show why the MVG direction code alone is not enough: 14 "H" leaves
// the city, 19 "H" goes into it).
std::vector<Section> pasingSections() {
  JsonDocument config;
  deserializeJson(config, R"({"sections":[
      {"name":"Stadt","lines":["S3:H:S","S4:H:S","S5:H:S","S6:H:S","S8:H:S","19:H:T"]},
      {"name":"Raus","lines":["S3:R:S","S4:R:S","S6:R:S","S8:R:S","14:H:T","RB6:H:Z"]}]})");
  core::ConfigIssues issues;
  const widgets::mvg::Config c = widgets::mvg::parseConfig(config.as<JsonObjectConst>(), issues);
  TEST_ASSERT_EQUAL_size_t(0, issues.size());
  return widgets::mvg::departureSections(load("pasing_20261019.json"), c, 9);
}

std::string readFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

void writeFile(const std::string& path, const std::string& data) {
  std::filesystem::create_directories(std::filesystem::path(path).parent_path());
  std::ofstream(path, std::ios::binary) << data;
}

ui::BitmapCanvas area(ui::Bitmap& bitmap, const ui::Rect& r) {
  return ui::BitmapCanvas(bitmap, r.x, r.y, r.w, r.h);
}

// Tile of a widget that does not exist yet.
void notSetUp(ui::Canvas& canvas, ui::Text title) {
  const ui::Language de = ui::Language::kGerman;
  ui::drawTileTitle(canvas, ui::text(de, title));
  ui::drawTileMessage(canvas, ui::text(de, ui::Text::kNotSetUp),
                      ui::text(de, ui::Text::kNotAvailable));
}

// Full screen: header, MVG column, weather and calendar tiles (these
// widgets do not exist yet).
ui::Bitmap screen(const std::vector<Section>& sections, const char* title, TimeFormat format,
                  int64_t now = kNow, const char* stale_since = nullptr) {
  ui::Bitmap bitmap(ui::kScreenWidth, ui::kScreenHeight);
  const ui::Layout layout = ui::layoutFor("one_plus_two");

  ui::BitmapCanvas header = area(bitmap, layout.header);
  widgets::header::draw(header, {now, ui::Language::kGerman, stale_since == nullptr});

  ui::BitmapCanvas column = area(bitmap, layout.slots[0]);
  widgets::mvg::ViewOptions options;
  options.title = title;
  options.time_format = format;
  options.now_s = now;
  if (stale_since != nullptr) {
    options.note = std::string("Stand ") + stale_since;
    options.note_alert = true;
  }
  widgets::mvg::drawDepartures(column, sections, options);

  ui::BitmapCanvas weather = area(bitmap, layout.slots[1]);
  notSetUp(weather, ui::Text::kTitleWeather);
  ui::BitmapCanvas calendar = area(bitmap, layout.slots[2]);
  notSetUp(calendar, ui::Text::kTitleCalendar);

  ui::BitmapCanvas full(bitmap);
  ui::drawSeparators(full, layout);
  return bitmap;
}

void check(const ui::Bitmap& bitmap, const char* name) {
  const std::string png =
      hal::png::encode(bitmap.data(), bitmap.width(), bitmap.height(), bitmap.stride());
  writeFile(std::string(".pio/preview/") + name + ".png", png);
  const std::string reference = std::string("test/fixtures/ui/") + name + ".png";
  if (std::getenv("INKTAVLA_UPDATE_PREVIEWS") != nullptr) {
    writeFile(reference, png);
    return;
  }
  const std::string expected = readFile(reference);
  TEST_ASSERT_TRUE_MESSAGE(!expected.empty(), ("missing reference " + reference).c_str());
  TEST_ASSERT_TRUE_MESSAGE(hal::png::criticalChunks(expected) == png,
                           ("differs from " + reference + ", see .pio/preview/").c_str());
}

}  // namespace

void setUp() { core::setTimeZone(core::kDefaultTimeZone); }
void tearDown() {}

void test_screen_pasing_sections() {
  check(screen(pasingSections(), "Pasing", TimeFormat::kClock), "mvg_pasing_sections");
}

void test_screen_pasing_stale() {
  // Old data after a failed update, Wi-Fi lost.
  check(screen(pasingSections(), "Pasing", TimeFormat::kClock, kNow, "22:21"), "mvg_pasing_stale");
}

void test_screen_pasing_list() {
  check(screen(list("pasing_20261019.json"), "Pasing", TimeFormat::kClock), "mvg_pasing_list");
}

void test_screen_donnersbergerbruecke_clock() {
  check(
      screen(list("donnersbergerbruecke_20261019.json"), "Donnersbergerbrücke", TimeFormat::kClock),
      "mvg_donnersbergerbruecke_clock");
}

void test_screen_holzkirchen_minutes() {
  // Cancellation, coupled trains, rail replacement bus; minutes mode.
  check(screen(list("holzkirchen_20261019.json"), "Holzkirchen", TimeFormat::kMinutes),
        "mvg_holzkirchen_minutes");
}

void test_screen_split_trains() {
  // Hand-written cases: three destinations shortened evenly, partial
  // cancellation, buses (07:00 local time, before the first departure).
  check(
      screen(list("rules_split_trains.json"), "Split trains", TimeFormat::kClock, 1792386000 - 60),
      "mvg_split_trains_clock");
}

void test_empty_list() {
  ui::Bitmap bitmap(358, 438);
  ui::BitmapCanvas canvas(bitmap);
  widgets::mvg::ViewOptions options;
  options.title = "Holzkirchen";
  TEST_ASSERT_EQUAL_INT(0, widgets::mvg::drawDepartures(canvas, {{"", {}}}, options));
  check(bitmap, "mvg_empty");
}

void test_empty_section() {
  ui::Bitmap bitmap(358, 438);
  ui::BitmapCanvas canvas(bitmap);
  std::vector<Section> sections = pasingSections();
  sections[1].departures.clear();
  widgets::mvg::ViewOptions options;
  options.title = "Pasing";
  options.now_s = kNow;
  TEST_ASSERT_EQUAL_INT(8, widgets::mvg::drawDepartures(canvas, sections, options));
  check(bitmap, "mvg_empty_section");
}

void test_mvv_line_icons() {
  // MVV symbols (off by default until phase B).
  ui::Bitmap bitmap(358, 438);
  ui::BitmapCanvas canvas(bitmap);
  widgets::mvg::ViewOptions options;
  options.title = "Pasing";
  options.now_s = kNow;
  options.line_icons = true;
  widgets::mvg::drawDepartures(canvas, list("pasing_20261019.json"), options);
  check(bitmap, "mvg_line_icons");
}

void test_not_set_up_tile() {
  ui::Bitmap bitmap(358, 438);
  ui::BitmapCanvas canvas(bitmap);
  ui::drawTileTitle(canvas, "Abfahrten");
  ui::drawTileMessage(canvas, "Haltestelle fehlt", "config.json: widgets.mvg.station");
  check(bitmap, "tile_not_set_up");
}

void test_rows_that_fit() {
  TEST_ASSERT_EQUAL_INT(10, widgets::mvg::listRowsThatFit(438));
  TEST_ASSERT_EQUAL_INT(9, widgets::mvg::sectionRowsThatFit(438, 2));
  TEST_ASSERT_EQUAL_INT(0, widgets::mvg::listRowsThatFit(30));
  TEST_ASSERT_EQUAL_INT(0, widgets::mvg::sectionRowsThatFit(438, 0));
}

void test_split_rows() {
  using V = std::vector<int>;
  TEST_ASSERT_TRUE(widgets::mvg::splitRows(9, V{8, 8}) == (V{5, 4}));
  TEST_ASSERT_TRUE(widgets::mvg::splitRows(9, V{2, 8}) == (V{2, 7}));
  TEST_ASSERT_TRUE(widgets::mvg::splitRows(9, V{8, 0}) == (V{8, 1}));  // keeps a row for the note
  TEST_ASSERT_TRUE(widgets::mvg::splitRows(9, V{1, 1}) == (V{1, 1}));
  TEST_ASSERT_TRUE(widgets::mvg::splitRows(0, V{3}) == (V{0}));
}

// --- Briefing building blocks (sheets to look at, also reference images) -------

void caption(ui::Canvas& c, int centre_x, int baseline, const char* text) {
  ui::drawText(c, ui::fonts::kRobotoCondensedMedium13, centre_x, baseline, text, ui::Color::kBlack,
               ui::Align::kCenter);
}

// All symbols of the Briefing design: the large weather pictures of the
// band, the weather symbols of the columns and the small symbols.
void test_briefing_symbols() {
  ui::Bitmap bitmap(ui::kScreenWidth, ui::kScreenHeight);
  bitmap.clear(ui::Color::kWhite);
  ui::BitmapCanvas c(bitmap);
  using W = ui::WeatherIcon;
  struct Named {
    W kind;
    const char* name;
  };
  const Named band[] = {{W::kSun, "Sonne"},
                        {W::kPartlyCloudy, "Heiter"},
                        {W::kCloudy, "Wolken"},
                        {W::kRain, "Regen"},
                        {W::kFog, "Nebel"},
                        {W::kMoon, "Klar (Nacht)"},
                        {W::kPartlyCloudyNight, "Heiter (Nacht)"},
                        {W::kSnow, "Schnee"},
                        {W::kThunder, "Gewitter"}};
  for (int i = 0; i < 9; i++) {
    const int x = 4 + (i % 5) * 159;
    const int y = 4 + (i / 5) * 150;
    ui::drawIcon(c, x, y, ui::bandIcon(band[i].kind));
    caption(c, x + ui::kBandIconWidth / 2, y + ui::kBandIconHeight + 12, band[i].name);
  }
  // Weather symbols of the columns, 30 px.
  for (int i = 0; i < 9; i++) {
    const int x = 20 + i * 44;
    ui::drawIcon(c, x, 318, ui::weatherIcon(band[i].kind, 30));
  }
  caption(c, 214, 364, "Wettersymbole 30 px (Vorhersagetage)");

  // Small symbols.
  ui::drawIcon(c, 440, 330, ui::kCalendarIcon);
  caption(c, 447, 364, "ganztägig");
  ui::drawIcon(c, 500, 329, ui::kPinIcon);
  caption(c, 508, 364, "Haltestelle");
  ui::drawColumnTitle(c, 560, 600, 344, "", "", ui::TitleIcon::kArrow);
  caption(c, 566, 364, "Richtung");
  ui::drawWarningSign(c, 620, 345, 15);
  ui::drawWarningSign(c, 646, 345, 18);
  caption(c, 643, 364, "Warnung 15/18");

  // Line symbols, 22 px high (Bold 17), and a chain for journeys.
  const ui::Font& f = ui::fonts::kRobotoCondensedBold17;
  struct Line {
    const char* label;
    ui::SymbolShape shape;
    const char* name;
  };
  const Line lines[] = {{"S4", ui::SymbolShape::kPill, "S-Bahn"},
                        {"U6", ui::SymbolShape::kRect, "U-Bahn"},
                        {"19", ui::SymbolShape::kBox, "Tram"},
                        {"RB55", ui::SymbolShape::kFrame, "Regionalzug"},
                        {"56", ui::SymbolShape::kRoundFrame, "Bus/SEV"}};
  int x = 20;
  for (const Line& l : lines) {
    const int w = std::max(44, ui::textWidth(f, l.label) + 16);
    ui::drawLineSymbol(c, f, x, 392, w, 22, l.label, l.shape);
    caption(c, x + w / 2, 430, l.name);
    x += w + 36;
  }
  ui::drawLineSymbolChain(c, ui::fonts::kRobotoCondensedBold15, ui::fonts::kRobotoCondensedMedium13,
                          470, 403, 20,
                          {{"19", ui::SymbolShape::kBox, 0, "5 min"},
                           {"U2", ui::SymbolShape::kRect, 0, "12 min"},
                           {"RB55", ui::SymbolShape::kFrame, 0, ""}});
  caption(c, 560, 450, "Kette (Verbindungen, später)");
  check(bitmap, "briefing_symbols");
}

// Fonts and building blocks of the Briefing design, arranged roughly like
// the screen (not the finished layout).
void test_briefing_parts() {
  ui::Bitmap bitmap(ui::kScreenWidth, ui::kScreenHeight);
  bitmap.clear(ui::Color::kWhite);
  ui::BitmapCanvas c(bitmap);
  // Band: header, sentence (wrapped), weather picture.
  ui::drawTrackedText(c, ui::fonts::kRobotoCondensedBold16, 28, 34,
                      ui::toUpper("Dienstag, 20. Oktober · 07:42"), 16);
  const std::vector<std::string> sentence = ui::wrapText(
      ui::fonts::kFraunces38,
      "Guten Morgen. Bis Mittag bleibt es trocken, ab 14 Uhr regnet es – Schirm mit.", 564, 3);
  for (size_t i = 0; i < sentence.size(); i++) {
    ui::drawText(c, ui::fonts::kFraunces38, 26, 82 + 44 * static_cast<int>(i), sentence[i]);
  }
  ui::drawIcon(c, 622, 18, ui::bandIcon(ui::WeatherIcon::kRain));
  ui::drawText(c, ui::fonts::kFraunces24Medium, 630, 186, "7° bis 14°");
  c.hLine(28, 204, 744, ui::Color::kBlack);

  // Departure column: titles, disruption text.
  ui::drawWarningSign(c, 28, 240, 18);
  ui::drawText(c, ui::fonts::kRobotoCondensedBold17, 52, 234, "Signalstörung bei Laim");
  const std::vector<std::string> text =
      ui::wrapText(ui::fonts::kRobotoCondensedMedium17,
                   "Ausfälle und Verspätungen Richtung Innenstadt", 292 - 52, 3);
  for (size_t i = 0; i < text.size(); i++) {
    ui::drawText(c, ui::fonts::kRobotoCondensedMedium17, 52, 254 + 20 * static_cast<int>(i),
                 text[i]);
  }
  ui::drawColumnTitle(c, 28, 292, 322, "In die Stadt", "", ui::TitleIcon::kArrow);
  ui::drawColumnTitle(c, 28, 292, 352, "Pasing", "Bahnhof", ui::TitleIcon::kPin);
  ui::drawColumnTitle(c, 28, 292, 382, "Pasing Marienpl.", "4 min zu Fuß", ui::TitleIcon::kPin);
  ui::drawColumnTitle(c, 28, 292, 412, "Raus", "Stand 07:41", ui::TitleIcon::kArrow, true);
  ui::drawText(c, ui::fonts::kRobotoCondensedBold21, 262, 446, "07:47", ui::Color::kBlack,
               ui::Align::kRight);
  ui::drawText(c, ui::fonts::kRobotoCondensedBold15, 265, 446, "+8");

  // Calendar column: Fraunces 20 (next event bold), "…", week rows in 18.
  const int x0 = 318, x1 = 532;
  ui::drawColumnTitle(c, x0, x1, 234, "Heute", "KW 43");
  ui::drawText(c, ui::fonts::kRobotoCondensedBold17, x0, 264, "08:30");
  ui::drawText(c, ui::fonts::kFraunces20Bold, x0 + 50, 264, "Team-Standup");
  ui::drawText(c, ui::fonts::kRobotoCondensedMedium17, x0, 291, "12:30");
  ui::drawText(c, ui::fonts::kFraunces20, x0 + 50, 291,
               ui::fitTextEllipsis(ui::fonts::kFraunces20, "Mittagessen mit Jonas", x1 - x0 - 50));
  ui::drawIcon(c, x0 + 2, 304, ui::kCalendarIcon);
  ui::drawText(c, ui::fonts::kFraunces20, x0 + 50, 318, "Papiertonne");
  c.dottedHLine(x0, 328, x1 - x0 + 1, 2);
  ui::drawText(c, ui::fonts::kRobotoCondensedBold16, x0, 350, "Do");
  ui::drawText(c, ui::fonts::kFraunces18, x0 + 50, 350, "Chorprobe");
  ui::drawText(c, ui::fonts::kRobotoCondensedMedium15, x1, 350, "19:00", ui::Color::kBlack,
               ui::Align::kRight);

  // Weather column: title, screened rain bars, day cell, credit.
  const int w0 = 558, w1 = 772;
  ui::drawColumnTitle(c, w0, w1, 234, "Wetter", "München");
  for (int i = 0; i < 6; i++) c.fillDotted(w0 + 40 + i * 12, 300 - i * 5, 10, i * 5 + 4);
  c.hLine(w0 + 6, 300, w1 - w0 - 12, ui::Color::kBlack);
  ui::drawText(c, ui::fonts::kRobotoCondensedBold16, w0 + 35, 360, "Mi", ui::Color::kBlack,
               ui::Align::kCenter);
  ui::drawIcon(c, w0 + 20, 366, ui::weatherIcon(ui::WeatherIcon::kPartlyCloudy, 30));
  const int hx = ui::drawText(c, ui::fonts::kRobotoCondensedBold17, w0 + 14, 418, "15°");
  ui::drawText(c, ui::fonts::kRobotoCondensedMedium16, hx, 418, " 6°");
  ui::drawText(c, ui::fonts::kRobotoCondensedMedium13, w1, 464, "Open-Meteo", ui::Color::kBlack,
               ui::Align::kRight);
  check(bitmap, "briefing_parts");
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_screen_pasing_sections);
  RUN_TEST(test_screen_pasing_stale);
  RUN_TEST(test_screen_pasing_list);
  RUN_TEST(test_screen_donnersbergerbruecke_clock);
  RUN_TEST(test_screen_holzkirchen_minutes);
  RUN_TEST(test_screen_split_trains);
  RUN_TEST(test_empty_list);
  RUN_TEST(test_empty_section);
  RUN_TEST(test_mvv_line_icons);
  RUN_TEST(test_not_set_up_tile);
  RUN_TEST(test_rows_that_fit);
  RUN_TEST(test_split_rows);
  RUN_TEST(test_briefing_symbols);
  RUN_TEST(test_briefing_parts);
  return UNITY_END();
}
