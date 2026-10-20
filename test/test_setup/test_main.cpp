// Smoke tests for the host test setup (pio test -e native).
//
// They check that the layer headers compile without Arduino, that their
// interfaces can be implemented, and that the test fixtures are reachable.

#include <unity.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "core/version.h"
#include "hal/display.h"
#include "ui/canvas.h"
#include "widgets/widget.h"

namespace {

class FakeCanvas : public ui::Canvas {
 public:
  uint16_t width() const override { return 320; }
  uint16_t height() const override { return 440; }
  void setPixel(int, int, ui::Color) override {}
};

class FakeWidget : public widgets::Widget {
 public:
  const char* id() const override { return "fake"; }
  std::string title(ui::Language) const override { return "Fake"; }
  bool fetches() const override { return true; }
  core::Schedule schedule() const override { return {60, -1, 10}; }
  void draw(ui::Canvas& canvas, const widgets::DrawContext&) override {
    last_width_ = canvas.width();
    last_height_ = canvas.height();
  }

  uint16_t last_width_ = 0;
  uint16_t last_height_ = 0;
};

// `pio test` runs the test program from the project directory.
std::string fixturePath(const char* relative) { return std::string("test/fixtures/") + relative; }

std::string readFile(const std::string& path) {
  std::string content;
  FILE* file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) return content;
  char buffer[512];
  size_t n;
  while ((n = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
    content.append(buffer, n);
  }
  std::fclose(file);
  return content;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_firmware_version_is_set() {
  TEST_ASSERT_GREATER_THAN(0, std::strlen(core::kFirmwareVersion));
}

void test_widget_draws_through_canvas() {
  FakeCanvas canvas;
  FakeWidget widget;
  widgets::Widget& w = widget;

  w.draw(canvas, widgets::DrawContext{});

  TEST_ASSERT_EQUAL_STRING("fake", w.id());
  TEST_ASSERT_EQUAL_UINT32(60, w.schedule().interval_s);
  TEST_ASSERT_TRUE(w.isSetUp());
  TEST_ASSERT_TRUE(w.fetch(0));
  TEST_ASSERT_EQUAL_UINT16(320, widget.last_width_);
  TEST_ASSERT_EQUAL_UINT16(440, widget.last_height_);
}

void test_mvg_fixtures_are_readable() {
  const char* files[] = {"mvg/pasing_deps.json",
                         "mvg/pasing_lines.json",
                         "mvg/hk_deps.json",
                         "mvg/hk_lines.json",
                         "mvg/parse_deps.json",
                         "mvg/rules_messages.json",
                         "mvg/rules_early_termination.json",
                         "mvg/rules_split_trains.json",
                         "mvg/rules_sorting.json"};
  for (const char* name : files) {
    const std::string content = readFile(fixturePath(name));
    TEST_ASSERT_TRUE_MESSAGE(!content.empty(), name);
    TEST_ASSERT_EQUAL_CHAR_MESSAGE('[', content.front(), name);  // JSON array
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_firmware_version_is_set);
  RUN_TEST(test_widget_draws_through_canvas);
  RUN_TEST(test_mvg_fixtures_are_readable);
  return UNITY_END();
}
