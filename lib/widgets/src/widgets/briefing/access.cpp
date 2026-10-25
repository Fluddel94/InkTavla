#include "widgets/briefing/access.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "core/time.h"
#include "ui/fonts.h"
#include "ui/layout.h"
#include "ui/qr.h"
#include "ui/text.h"
#include "widgets/briefing/band.h"

namespace widgets::briefing {

namespace {

using ui::Color;
namespace fonts = ui::fonts;

// "14:32" of a Unix time, "" if unknown.
std::string clockText(int64_t wall_s) {
  core::LocalTime t;
  if (wall_s <= 0 || !core::isValidUnixTime(wall_s) || !core::toLocalTime(wall_s, t)) return "";
  char buffer[8];
  std::snprintf(buffer, sizeof(buffer), "%02d:%02d", t.hour, t.minute);
  return buffer;
}

// Address without "http://" for display.
std::string shortAddress(const std::string& address) {
  const std::string prefix = "http://";
  return address.compare(0, prefix.size(), prefix) == 0 ? address.substr(prefix.size()) : address;
}

// Label in Medium and value in Bold on one baseline; returns the x after.
int labelValue(ui::Canvas& canvas, int x, int baseline, const char* label, const std::string& value,
               const ui::Font& label_font, const ui::Font& value_font, int max_x) {
  int end = ui::drawText(canvas, label_font, x, baseline, label);
  end += 8;
  const std::string fitted = ui::fitText(value_font, value, max_x - end);
  return ui::drawText(canvas, value_font, end, baseline, fitted);
}

// "482 913": a code of six characters in two groups, easier to read.
std::string groupedCode(const std::string& code) {
  return code.size() == 6 ? code.substr(0, 3) + " " + code.substr(3) : code;
}

}  // namespace

void drawSettingsBand(ui::Canvas& canvas, const DrawContext& context,
                      const core::AccessNotice& notice) {
  const ui::Language lang = context.language;
  drawHeaderLine(canvas, context);

  // QR code on the right, where the weather picture is otherwise.
  constexpr int kQrRight = ui::kScreenWidth - kHeaderX;
  constexpr int kQrTop = 46;
  constexpr int kQrMax = kRuleY - 6 - kQrTop;  // up to just above the rule
  int text_right = kHeaderX + kSentenceWidth;
  const ui::QrCode code(notice.qr);
  if (code.size() > 0) {
    const int module = std::min(ui::qrModuleFor(code, kQrMax, 1), 5);
    const int side = ui::qrPixelSize(code, module, 1);
    ui::drawQrCode(canvas, kQrRight - side, kQrTop + (kQrMax - side) / 2, code, module, 1);
    text_right = std::min(text_right, kQrRight - side - 24);
  }

  ui::drawText(canvas, fonts::kFraunces34, kSentenceX, 80,
               ui::fitTextEllipsis(fonts::kFraunces34, ui::text(lang, ui::Text::kSettingsOpen),
                                   text_right - kSentenceX));
  labelValue(canvas, kHeaderX, 120, ui::text(lang, ui::Text::kSettingsBrowser),
             shortAddress(notice.address), fonts::kRobotoCondensedMedium20,
             fonts::kRobotoCondensedBold21, text_right);
  labelValue(canvas, kHeaderX, 152, ui::text(lang, ui::Text::kSettingsCode),
             groupedCode(notice.code), fonts::kRobotoCondensedMedium20,
             fonts::kRobotoCondensedBold21, text_right);

  std::string footer = ui::text(lang, ui::Text::kSettingsQr);
  const std::string until = clockText(notice.closes_wall_s);
  if (!until.empty()) {
    footer += std::string(" ") + ui::text(lang, ui::Text::kOpenUntil) + " " + until + ".";
  }
  ui::drawText(canvas, fonts::kRobotoCondensedMedium16, kHeaderX, 186,
               ui::fitTextEllipsis(fonts::kRobotoCondensedMedium16, footer, text_right - kHeaderX));

  canvas.hLine(kHeaderX, kRuleY, ui::kScreenWidth - 2 * kHeaderX, Color::kBlack);
}

void drawWifiScreen(ui::Canvas& canvas, const DrawContext& context,
                    const core::AccessNotice& notice) {
  const ui::Language lang = context.language;
  constexpr int kLeft = kHeaderX;
  constexpr int kRight = ui::kScreenWidth - kHeaderX;
  const bool saved = notice.mode == core::AccessMode::kWifiNotConnected;

  // Header line: "WLAN-EINRICHTUNG", the clock if known.
  std::string header = ui::toUpper(ui::text(lang, ui::Text::kSetupHeading));
  const std::string now = clockText(context.now_s);
  if (!now.empty()) header += " \u00b7 " + now;
  ui::drawTrackedText(canvas, headerFont(), kLeft, kHeaderBaseline, header, 16);

  const ui::Text title = saved ? ui::Text::kWifiNotConnectedTitle : ui::Text::kWifiNotSetUpTitle;
  ui::drawText(canvas, fonts::kFraunces38, kSentenceX, 88, ui::text(lang, title));
  canvas.hLine(kLeft, 112, kRight - kLeft, Color::kBlack);

  const ui::Font& text_font = fonts::kRobotoCondensedMedium20;
  const ui::Font& bold = fonts::kRobotoCondensedBold21;
  constexpr int kTextX = kLeft + 34;
  constexpr int kLine = 28;
  int y = 158;
  if (saved && !notice.network.empty()) {
    labelValue(canvas, kLeft, y, ui::text(lang, ui::Text::kWifiSaved), notice.network, text_font,
               bold, kRight);
    y += kLine + 22;
  }

  // Steps: number in Fraunces, text wrapped to the width.
  const ui::Text steps[] = {ui::Text::kSetupStep1, ui::Text::kSetupStep2, ui::Text::kSetupStep3};
  for (int i = 0; i < 3; i++) {
    ui::drawText(canvas, fonts::kFraunces24Medium, kLeft, y, std::to_string(i + 1));
    for (const std::string& line :
         ui::wrapText(text_font, ui::text(lang, steps[i]), kRight - kTextX, 2)) {
      ui::drawText(canvas, text_font, kTextX, y, line);
      y += kLine;
    }
    y += 14;
  }

  if (saved) {
    ui::drawText(canvas, fonts::kRobotoCondensedMedium16, kLeft, ui::kBriefingContentBottom,
                 ui::text(lang, ui::Text::kWifiRetrying));
  }
}

}  // namespace widgets::briefing
