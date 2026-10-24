#pragma once

// Notices while the device can be reached for setting up (see
// core/access_notice.h), in the style of the "Briefing" layout:
//
// - Settings page open: the band keeps its header line; instead of the
//   sentence it shows the address, the one-time code and until when the
//   page is open, and a QR code (address with code) instead of the weather
//   picture. The columns below keep running.
// - No Wi-Fi (none saved, or the saved one not reached since the start):
//   the whole screen with the steps to set up Wi-Fi over the USB cable
//   (Improv), and the name of the saved network.

#include "core/access_notice.h"
#include "ui/canvas.h"
#include "widgets/widget.h"

namespace widgets::briefing {

// Into the band (800 x 205).
void drawSettingsBand(ui::Canvas& canvas, const DrawContext& context,
                      const core::AccessNotice& notice);

// Into the whole screen (800 x 480).
void drawWifiScreen(ui::Canvas& canvas, const DrawContext& context,
                    const core::AccessNotice& notice);

}  // namespace widgets::briefing
