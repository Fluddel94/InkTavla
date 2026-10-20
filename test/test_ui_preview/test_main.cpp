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
#include "png.h"
#include "ui/bitmap.h"
#include "ui/fonts.h"
#include "ui/layout.h"
#include "ui/text.h"
#include "widgets/mvg/departures.h"
#include "widgets/mvg/view.h"

namespace {

constexpr int64_t kNow = 1792441500;  // 2026-10-19 22:25 CEST

std::vector<widgets::mvg::Departure> departures(const char* fixture) {
  std::ifstream file(std::string("test/fixtures/mvg/") + fixture, std::ios::binary);
  JsonDocument doc;
  widgets::mvg::readDepartures(file, doc);
  widgets::mvg::LineSelection all;
  return widgets::mvg::parseDepartures(doc, widgets::mvg::DirectionFilter::kAll, all, 20);
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

// Placeholder for widgets that do not exist yet.
void placeholder(ui::Bitmap& screen, const ui::Rect& r, const char* label) {
  ui::BitmapCanvas c(screen, r.x, r.y, r.w, r.h);
  ui::drawText(c, ui::fonts::kRobotoCondensedMedium19, r.w / 2, r.h / 2, label, ui::Color::kBlack,
               ui::Align::kCenter);
}

// Full screen: header, MVG column, two placeholder tiles.
ui::Bitmap screen(const char* fixture, const char* title, widgets::mvg::TimeFormat format,
                  int64_t now = kNow) {
  ui::Bitmap bitmap(ui::kScreenWidth, ui::kScreenHeight);
  const ui::Layout layout = ui::layoutFor("one_plus_two");

  ui::BitmapCanvas header(bitmap, layout.header.x, layout.header.y, layout.header.w,
                          layout.header.h);
  const ui::Font& bold = ui::fonts::kRobotoCondensedBold27;
  const int baseline = (layout.header.h + bold.cap_height) / 2;
  ui::drawText(header, bold, 8, baseline, "Mo 19.10.");
  ui::drawText(header, bold, header.width() / 2, baseline, "22:25", ui::Color::kBlack,
               ui::Align::kCenter);

  ui::BitmapCanvas column(bitmap, layout.slots[0].x, layout.slots[0].y, layout.slots[0].w,
                          layout.slots[0].h);
  widgets::mvg::ViewOptions options;
  options.title = title;
  options.time_format = format;
  options.now_s = now;
  widgets::mvg::drawDepartures(column, departures(fixture), options);

  placeholder(bitmap, layout.slots[1], "Weather (not implemented yet)");
  placeholder(bitmap, layout.slots[2], "Calendar (not implemented yet)");
  ui::BitmapCanvas full(bitmap);
  ui::drawSeparators(full, layout);
  return bitmap;
}

void check(const ui::Bitmap& bitmap, const char* name) {
  const std::string png = preview::encodePng(bitmap);
  writeFile(std::string(".pio/preview/") + name + ".png", png);
  const std::string reference = std::string("test/fixtures/ui/") + name + ".png";
  if (std::getenv("INKTAVLA_UPDATE_PREVIEWS") != nullptr) {
    writeFile(reference, png);
    return;
  }
  const std::string expected = readFile(reference);
  TEST_ASSERT_TRUE_MESSAGE(!expected.empty(), ("missing reference " + reference).c_str());
  TEST_ASSERT_TRUE_MESSAGE(expected == png,
                           ("differs from " + reference + ", see .pio/preview/").c_str());
}

}  // namespace

void setUp() { core::setTimeZone(core::kDefaultTimeZone); }
void tearDown() {}

void test_screen_donnersbergerbruecke_minutes() {
  check(screen("donnersbergerbruecke_20261019.json", "Donnersbergerbrücke",
               widgets::mvg::TimeFormat::kMinutes),
        "mvg_donnersbergerbruecke_minutes");
}

void test_screen_donnersbergerbruecke_clock() {
  check(screen("donnersbergerbruecke_20261019.json", "Donnersbergerbrücke",
               widgets::mvg::TimeFormat::kClock),
        "mvg_donnersbergerbruecke_clock");
}

void test_screen_holzkirchen_minutes() {
  // Cancellation, coupled trains, rail replacement bus.
  check(screen("holzkirchen_20261019.json", "Holzkirchen", widgets::mvg::TimeFormat::kMinutes),
        "mvg_holzkirchen_minutes");
}

void test_screen_split_trains() {
  // Hand-written cases: three destinations shortened evenly, partial
  // cancellation, buses (07:00 local time, before the first departure).
  check(screen("rules_split_trains.json", "Split trains", widgets::mvg::TimeFormat::kClock,
               1792386000 - 60),
        "mvg_split_trains_clock");
}

void test_empty_list() {
  ui::Bitmap bitmap(358, 438);
  ui::BitmapCanvas canvas(bitmap);
  widgets::mvg::ViewOptions options;
  options.title = "Holzkirchen";
  TEST_ASSERT_EQUAL_INT(0, widgets::mvg::drawDepartures(canvas, {}, options));
  check(bitmap, "mvg_empty");
}

void test_rows_that_fit() {
  TEST_ASSERT_EQUAL_INT(8, widgets::mvg::rowsThatFit(438));
  TEST_ASSERT_EQUAL_INT(0, widgets::mvg::rowsThatFit(40));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_screen_donnersbergerbruecke_minutes);
  RUN_TEST(test_screen_donnersbergerbruecke_clock);
  RUN_TEST(test_screen_holzkirchen_minutes);
  RUN_TEST(test_screen_split_trains);
  RUN_TEST(test_empty_list);
  RUN_TEST(test_rows_that_fit);
  return UNITY_END();
}
