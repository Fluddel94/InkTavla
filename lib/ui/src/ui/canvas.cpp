#include "ui/canvas.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ui {

namespace {

// Horizontal inset of a row of a rounded rectangle (corner radius r).
int cornerInset(int row, int height, int r) {
  const int dy = row < r ? r - row : (row >= height - r ? row - (height - 1 - r) : 0);
  if (dy <= 0) return 0;
  const double cy = dy - 0.5;
  const double dx2 = static_cast<double>(r) * r - cy * cy;
  const double dx = dx2 > 0 ? std::sqrt(dx2) : 0;
  return static_cast<int>(r - dx + 0.5);
}

}  // namespace

void Canvas::fillRect(int x, int y, int w, int h, Color color) {
  const int x0 = std::max(x, 0);
  const int y0 = std::max(y, 0);
  const int x1 = std::min(x + w, static_cast<int>(width()));
  const int y1 = std::min(y + h, static_cast<int>(height()));
  for (int py = y0; py < y1; py++) {
    for (int px = x0; px < x1; px++) setPixel(px, py, color);
  }
}

void Canvas::dottedHLine(int x, int y, int w, int step, Color color) {
  if (step < 1) step = 1;
  for (int i = 0; i < w; i += step) setPixel(x + i, y, color);
}

void Canvas::dottedVLine(int x, int y, int h, int step, Color color) {
  if (step < 1) step = 1;
  for (int i = 0; i < h; i += step) setPixel(x, y + i, color);
}

void Canvas::fillDotted(int x, int y, int w, int h, int step, Color color) {
  if (step < 1) step = 1;
  const int x0 = std::max(0, x);
  const int y0 = std::max(0, y);
  for (int py = y0 + (step - y0 % step) % step; py < y + h; py += step) {
    for (int px = x0 + (step - x0 % step) % step; px < x + w; px += step) {
      setPixel(px, py, color);
    }
  }
}

void Canvas::drawRect(int x, int y, int w, int h, Color color, int thickness) {
  if (w <= 0 || h <= 0) return;
  const int t = std::min({thickness, (w + 1) / 2, (h + 1) / 2});
  fillRect(x, y, w, t, color);
  fillRect(x, y + h - t, w, t, color);
  fillRect(x, y + t, t, h - 2 * t, color);
  fillRect(x + w - t, y + t, t, h - 2 * t, color);
}

void Canvas::drawLine(int x0, int y0, int x1, int y1, Color color) {
  const int dx = std::abs(x1 - x0);
  const int dy = -std::abs(y1 - y0);
  const int sx = x0 < x1 ? 1 : -1;
  const int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    setPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    const int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void Canvas::fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, Color color) {
  // Scanline fill: for each row, the span between the edges crossing it.
  // Pixel centres (x + 0.5, y + 0.5) inside the triangle are set.
  const int top = std::min({y0, y1, y2});
  const int bottom = std::max({y0, y1, y2});
  const int xs[3] = {x0, x1, x2};
  const int ys[3] = {y0, y1, y2};
  for (int y = top; y <= bottom; y++) {
    const double cy = y + 0.5;
    double left = 1e9;
    double right = -1e9;
    for (int i = 0; i < 3; i++) {
      const int j = (i + 1) % 3;
      const double ya = ys[i] + 0.5;
      const double yb = ys[j] + 0.5;
      if ((cy < ya && cy < yb) || (cy > ya && cy > yb)) continue;
      if (ya == yb) {
        left = std::min(left, std::min(xs[i], xs[j]) + 0.5);
        right = std::max(right, std::max(xs[i], xs[j]) + 0.5);
        continue;
      }
      const double x = xs[i] + 0.5 + (cy - ya) * (xs[j] - xs[i]) / (yb - ya);
      left = std::min(left, x);
      right = std::max(right, x);
    }
    if (left > right) continue;
    const int from = static_cast<int>(std::ceil(left - 0.5));
    const int to = static_cast<int>(std::floor(right - 0.5));
    for (int x = from; x <= to; x++) setPixel(x, y, color);
  }
}

void Canvas::fillCircle(int cx, int cy, int radius, Color color) {
  // Pixels whose centre lies within radius + 0.5 of the centre pixel's centre.
  const double r2 = (radius + 0.5) * (radius + 0.5);
  for (int dy = -radius; dy <= radius; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      if (dx * dx + dy * dy <= r2) setPixel(cx + dx, cy + dy, color);
    }
  }
}

void Canvas::drawCircle(int cx, int cy, int radius, int thickness, Color color) {
  const double outer = (radius + 0.5) * (radius + 0.5);
  const double inner = std::max(0.0, radius + 0.5 - thickness);
  for (int dy = -radius; dy <= radius; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      const int d2 = dx * dx + dy * dy;
      if (d2 <= outer && d2 > inner * inner) setPixel(cx + dx, cy + dy, color);
    }
  }
}

void Canvas::drawThickLine(int x0, int y0, int x1, int y1, int width, Color color) {
  if (width <= 1) {
    drawLine(x0, y0, x1, y1, color);
    return;
  }
  // Stamp a small disc along the line (Bresenham).
  const double r = width / 2.0;
  const int reach = static_cast<int>(r);
  const int dx = std::abs(x1 - x0);
  const int dy = -std::abs(y1 - y0);
  const int sx = x0 < x1 ? 1 : -1;
  const int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    for (int oy = -reach; oy <= reach; oy++) {
      for (int ox = -reach; ox <= reach; ox++) {
        // Even widths: disc between pixel centres (offset by half a pixel).
        const double fx = width % 2 == 0 ? ox + 0.5 : ox;
        const double fy = width % 2 == 0 ? oy + 0.5 : oy;
        if (fx * fx + fy * fy <= r * r) setPixel(x0 + ox, y0 + oy, color);
      }
    }
    if (x0 == x1 && y0 == y1) break;
    const int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void Canvas::fillRoundRect(int x, int y, int w, int h, int radius, Color color) {
  if (w <= 0 || h <= 0) return;
  const int r = std::max(0, std::min({radius, w / 2, h / 2}));
  for (int row = 0; row < h; row++) {
    const int inset = cornerInset(row, h, r);
    fillRect(x + inset, y + row, w - 2 * inset, 1, color);
  }
}

void Canvas::drawRoundRect(int x, int y, int w, int h, int radius, Color color, int thickness) {
  if (w <= 0 || h <= 0) return;
  const int t = std::max(1, std::min({thickness, w / 2, h / 2}));
  const int r = std::max(0, std::min({radius, w / 2, h / 2}));
  const int ri = std::max(0, r - t);
  for (int row = 0; row < h; row++) {
    const int outer = cornerInset(row, h, r);
    if (row < t || row >= h - t) {
      fillRect(x + outer, y + row, w - 2 * outer, 1, color);
      continue;
    }
    const int inner = t + cornerInset(row - t, h - 2 * t, ri);
    fillRect(x + outer, y + row, inner - outer, 1, color);
    fillRect(x + w - inner, y + row, inner - outer, 1, color);
  }
}

}  // namespace ui
