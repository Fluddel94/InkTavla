#pragma once

#include <cstdint>
#include <vector>

namespace ui {

enum class Color : uint8_t {
  kWhite = 0,
  kBlack = 1,
};

// Point with fractions of a pixel. Pixel (x, y) covers the square from
// (x, y) to (x + 1, y + 1); its centre is (x + 0.5, y + 0.5).
struct PointF {
  double x = 0;
  double y = 0;
};

// Drawing surface handed to a widget for its slot.
//
// Coordinates start at (0, 0) in the top-left corner of the slot, and
// everything outside the slot is clipped, so a widget can never paint over
// another one. Implementations provide setPixel(); the other primitives are
// built on top of it.
class Canvas {
 public:
  virtual ~Canvas() = default;

  virtual uint16_t width() const = 0;
  virtual uint16_t height() const = 0;

  // Sets one pixel; coordinates outside the canvas are ignored.
  virtual void setPixel(int x, int y, Color color) = 0;

  // Fills a rectangle, clipped to the canvas. The default sets pixel by
  // pixel; implementations may do it faster.
  virtual void fillRect(int x, int y, int w, int h, Color color);

  void fill(Color color) { fillRect(0, 0, width(), height(), color); }
  void hLine(int x, int y, int w, Color color, int thickness = 1) {
    fillRect(x, y, w, thickness, color);
  }
  void vLine(int x, int y, int h, Color color, int thickness = 1) {
    fillRect(x, y, thickness, h, color);
  }
  // Dotted lines: one pixel every `step` pixels, starting at x (or y).
  void dottedHLine(int x, int y, int w, int step, Color color = Color::kBlack);
  void dottedVLine(int x, int y, int h, int step, Color color = Color::kBlack);
  // Screened area: every `step`-th pixel in both directions (at canvas
  // coordinates divisible by step), e.g. the rain bars of the weather chart.
  void fillDotted(int x, int y, int w, int h, int step = 2, Color color = Color::kBlack);
  // Outline inside the rectangle.
  void drawRect(int x, int y, int w, int h, Color color, int thickness = 1);
  // Line of one pixel width (Bresenham).
  void drawLine(int x0, int y0, int x1, int y1, Color color);
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, Color color);
  // Line through the points with the given width, round ends and joins,
  // e.g. for curves in charts. The points are not rounded: a pixel is set
  // when its centre is at most width / 2 from the line.
  void drawPolyline(const std::vector<PointF>& points, double width, Color color);
  // Circle around (cx, cy): filled, or as a ring of the given thickness.
  void fillCircle(int cx, int cy, int radius, Color color);
  void drawCircle(int cx, int cy, int radius, int thickness, Color color);
  void fillRoundRect(int x, int y, int w, int h, int radius, Color color);
  // Outline inside the rectangle with rounded corners.
  void drawRoundRect(int x, int y, int w, int h, int radius, Color color, int thickness = 1);
};

}  // namespace ui
