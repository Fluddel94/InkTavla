// Screen previews drawn with the real UI and widget code.
//
// Each test draws a picture (departures from real MVG data, sheets with the
// building blocks of the design), writes it as PNG to .pio/preview/ and
// compares it with the reference image in test/fixtures/ui/. After an intended change of the
// appearance, update the references with:
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
#include "widgets/mvg/column_view.h"
#include "widgets/mvg/config.h"
#include "widgets/mvg/departures.h"
#include "widgets/mvg/line_icons.h"
#include "widgets/mvg/sections.h"

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

// Departures from real responses in columns of the screen: minutes with a
// cancellation, coupled trains and a replacement bus (Holzkirchen), the
// clock at a busy station (Donnersbergerbrücke) and the hand-written split
// trains with buses (07:00 local time, before the first departure).
void test_departure_columns_from_real_data() {
  ui::Bitmap bitmap(3 * 280 + 2 * 4, 260);
  bitmap.clear(ui::Color::kWhite);
  struct Case {
    const char* fixture;
    const char* station;
    TimeFormat format;
    int64_t now;
  };
  const Case cases[] = {
      {"holzkirchen_20261019.json", "Holzkirchen", TimeFormat::kMinutes, kNow},
      {"donnersbergerbruecke_20261019.json", "Donnersbergerbrücke", TimeFormat::kClock, kNow},
      {"rules_split_trains.json", "Split trains", TimeFormat::kClock, 1792386000 - 60}};
  for (int i = 0; i < 3; i++) {
    ui::BitmapCanvas column(bitmap, i * 284, 0, 280, 260);
    widgets::mvg::ColumnOptions options;
    options.station = cases[i].station;
    options.time_format = cases[i].format;
    options.now_s = cases[i].now;
    TEST_ASSERT_EQUAL_INT(
        widgets::mvg::columnRowsThatFit(260, 1),
        widgets::mvg::drawDeparturesColumn(column, list(cases[i].fixture), options));
  }
  check(bitmap, "mvg_columns");
}

// --- Briefing building blocks (sheets to look at, also reference images) -------

void caption(ui::Canvas& c, int centre_x, int baseline, const char* text) {
  ui::drawText(c, ui::fonts::kRobotoCondensedMedium13, centre_x, baseline, text, ui::Color::kBlack,
               ui::Align::kCenter);
}

// All line symbols: every MVV original (drawn kLineIconWidth wide) and
// generated ones for regional trains, buses and replacement services.
void test_line_symbols() {
  ui::Bitmap bitmap(ui::kScreenWidth, 200);
  bitmap.clear(ui::Color::kWhite);
  ui::BitmapCanvas c(bitmap);
  const char* originals[] = {"S1",  "S2",  "S3",  "S4",  "S5",  "S6",  "S7",  "S8", "S20",
                             "U1",  "U2",  "U3",  "U4",  "U5",  "U6",  "U7",  "U8", "12",
                             "14",  "15",  "16",  "17",  "18",  "19",  "20",  "21", "22",
                             "23",  "24",  "25",  "26",  "27",  "28",  "29",  "37", "N15",
                             "N16", "N17", "N18", "N19", "N20", "N21", "N27", "N37"};
  int i = 0;
  for (const char* label : originals) {
    const widgets::mvg::LineIcon* icon = widgets::mvg::findLineIcon(label);
    TEST_ASSERT_NOT_NULL(icon);
    widgets::mvg::drawLineIcon(c, 12 + (i % 16) * 49, 10 + (i / 16) * 26, *icon);
    i++;
  }
  caption(c, 400, 100, "MVV-Originale, alle 36 px breit");
  struct Line {
    const char* label;
    bool bus;
  };
  const Line generated[] = {{"RB55", false}, {"RE80", false}, {"RB", false}, {"56", true},
                            {"130", true},   {"X731", true},  {"N40", true}, {"SEV", true},
                            {"S2", true},    {"S9", false},   {"U9", false}};
  int x = 12;
  for (const Line& l : generated) {
    x += widgets::mvg::drawLineSymbol(c, x, 130, l.label, l.bus) + 14;
  }
  caption(c, 400, 170,
          "Erzeugt wie im Abfahrtsdisplay: Regionalzug, Bus, SEV, Linien ohne Original");
  check(bitmap, "mvg_line_symbols");
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
  // Weather symbols of the forecast days, 44 px.
  for (int i = 0; i < 9; i++) {
    ui::drawIcon(c, 12 + i * 46, 312, ui::weatherIcon(band[i].kind, 44));
  }
  caption(c, 214, 364, "Wettersymbole 44 px (Vorhersagetage)");
  // Only for the forecast days: rain by intensity, showers, freezing rain.
  const W extra[] = {W::kRainLight, W::kRain, W::kRainHeavy, W::kShowers, W::kFreezingRain};
  for (int i = 0; i < 5; i++) {
    ui::drawIcon(c, 440 + i * 46, 400, ui::weatherIcon(extra[i], 44));
  }
  caption(c, 554, 452, "Regen schwach/m\u00e4\u00dfig/stark, Schauer, Gefrierregen");

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

  // Line symbols: MVV originals and generated ones (see mvg_line_symbols).
  struct Line {
    const char* label;
    bool bus;
    const char* name;
  };
  const Line lines[] = {{"S4", false, "S-Bahn"},
                        {"U6", false, "U-Bahn"},
                        {"19", false, "Tram"},
                        {"RB55", false, "Regionalzug"},
                        {"56", true, "Bus/SEV"}};
  int x = 20;
  for (const Line& l : lines) {
    const int w = widgets::mvg::drawLineSymbol(c, x, 395, l.label, l.bus);
    caption(c, x + w / 2, 430, l.name);
    x += w + 36;
  }
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
  ui::drawIcon(c, w0 + 16, 364, ui::weatherIcon(ui::WeatherIcon::kPartlyCloudy, 38));
  const int hx = ui::drawText(c, ui::fonts::kRobotoCondensedBold17, w0 + 14, 418, "15°");
  ui::drawText(c, ui::fonts::kRobotoCondensedMedium16, hx, 418, " 6°");
  ui::drawText(c, ui::fonts::kRobotoCondensedMedium13, w1, 464, "Open-Meteo", ui::Color::kBlack,
               ui::Align::kRight);
  check(bitmap, "briefing_parts");
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_departure_columns_from_real_data);
  RUN_TEST(test_line_symbols);
  RUN_TEST(test_briefing_symbols);
  RUN_TEST(test_briefing_parts);
  return UNITY_END();
}
