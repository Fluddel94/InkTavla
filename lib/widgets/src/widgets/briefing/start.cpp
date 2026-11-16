#include "widgets/briefing/start.h"

#include <algorithm>

#include "ui/fonts.h"
#include "ui/layout.h"
#include "ui/text.h"

namespace widgets::briefing {

namespace {

namespace fonts = ui::fonts;

constexpr char kWordMark[] = "InkTavla";
// Baseline of the word mark in the logo (Fraunces 96, chosen 2026-10-31).
constexpr int kWordMarkBaseline = 270;
constexpr char kSeparator[] = "  ·  ";

// "„Heimnetz“" or "\"Home\"".
std::string quoted(ui::Language language, const std::string& text) {
  return language == ui::Language::kGerman ? "„" + text + "“" : "“" + text + "”";
}

}  // namespace

void drawLogo(ui::Canvas& canvas, const ui::Font& font, int center_x, int baseline) {
  // Proportions of the logo drawing (InkTavlaLogo2), relative to the cap
  // height of the word mark.
  const int cap = font.cap_height;
  const int stroke = std::max(2, (cap * 42 + 500) / 1000);
  const int grid_w = (cap * 1258 + 500) / 1000;
  const int grid_h = (cap * 1118 + 500) / 1000;
  const int gap = (cap * 190 + 500) / 1000;
  const int rule_gap = (cap * 210 + 500) / 1000;
  const int word_w = ui::textWidth(font, kWordMark);
  const int total = grid_w + gap + word_w;
  const int x = center_x - total / 2;
  const int top = baseline - grid_h;
  const ui::Color black = ui::Color::kBlack;
  // Mark (2026-11-16, instead of the 2 x 2 grid): a band across the top and
  // two columns of equal width below it, the lower left one black. The line
  // under the band at 45.6 % of the height (centre).
  canvas.fillRect(x, top, grid_w, stroke, black);
  canvas.fillRect(x, baseline - stroke, grid_w, stroke, black);
  canvas.fillRect(x, top, stroke, grid_h, black);
  canvas.fillRect(x + grid_w - stroke, top, stroke, grid_h, black);
  const int mid_x = x + (grid_w - stroke) / 2;
  const int band_y = top + (grid_h * 456 + 500) / 1000 - stroke / 2;
  canvas.fillRect(x, band_y, grid_w, stroke, black);
  canvas.fillRect(mid_x, band_y, stroke, baseline - band_y, black);
  canvas.fillRect(x, band_y, mid_x - x + stroke, baseline - band_y, black);
  ui::drawText(canvas, font, x + grid_w + gap, baseline, kWordMark);
  canvas.fillRect(x, baseline + rule_gap, total, stroke, black);
}

void drawStartScreen(ui::Canvas& canvas, ui::Language language, const StartInfo& info,
                     bool waiting_for_wifi) {
  const int width = canvas.width();
  drawLogo(canvas, fonts::kFraunces96Bold, width / 2, kWordMarkBaseline);

  // Footer: "Version 0.1.0  ·  Daten werden geladen …", or the saved Wi-Fi
  // the device waits for (a long name is shortened).
  const ui::Font& font = fonts::kRobotoCondensedMedium16;
  const int max_width = width - 2 * ui::kMargin;
  std::string footer;
  if (!info.version.empty()) {
    footer =
        std::string(ui::text(language, ui::Text::kStartVersion)) + " " + info.version + kSeparator;
  }
  if (!waiting_for_wifi) {
    footer += ui::text(language, ui::Text::kStartLoading);
  } else {
    footer += ui::text(language, ui::Text::kStartConnecting);
    if (!info.network.empty()) {
      const std::string after = " …";
      const int room = max_width - ui::textWidth(font, footer + " " + quoted(language, "") + after);
      footer += " " + quoted(language, ui::fitTextEllipsis(font, info.network, room));
    }
    footer += " …";
  }
  footer = ui::fitTextEllipsis(font, footer, max_width);
  ui::drawText(canvas, font, (width - ui::textWidth(font, footer)) / 2,
               canvas.height() - ui::kMarginBottom, footer);
}

}  // namespace widgets::briefing
