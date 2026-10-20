// Screen previews drawn with the real UI and widget code.
//
// Each test renders a screen from real MVG data, writes it as PNG to
// .pio/preview/ and compares it with the reference image in
// test/fixtures/ui/. After an intended change of the appearance, update the
// references with:
//
//   INKTAVLA_UPDATE_PREVIEWS=1 pio test -e native -f test_ui_preview

#include <unity.h>

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
#include "ui/i18n.h"
#include "ui/layout.h"
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
  return UNITY_END();
}
