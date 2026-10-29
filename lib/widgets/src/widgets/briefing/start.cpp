#include "widgets/briefing/start.h"

#include "ui/fonts.h"
#include "ui/layout.h"
#include "ui/text.h"

namespace widgets::briefing {

namespace {

namespace fonts = ui::fonts;

constexpr char kWordMark[] = "InkTavla";
constexpr int kWordMarkBaseline = 262;
constexpr int kRuleY = 300;  // short rule below the word mark
constexpr int kRuleWidth = 48;
constexpr char kSeparator[] = "  ·  ";

// "„Heimnetz“" or "\"Home\"".
std::string quoted(ui::Language language, const std::string& text) {
  return language == ui::Language::kGerman ? "„" + text + "“" : "“" + text + "”";
}

}  // namespace

void drawStartScreen(ui::Canvas& canvas, ui::Language language, const StartInfo& info,
                     bool waiting_for_wifi) {
  const int width = canvas.width();
  const ui::Font& logo = fonts::kFraunces96Bold;
  ui::drawText(canvas, logo, (width - ui::textWidth(logo, kWordMark)) / 2, kWordMarkBaseline,
               kWordMark);
  canvas.hLine((width - kRuleWidth) / 2, kRuleY, kRuleWidth, ui::Color::kBlack, 2);

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
    footer += ui::text(language, ui::Text::kStartWaitingForWifi);
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
