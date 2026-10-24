#pragma once

// Panel of the reference hardware: Seeed TRMNL 7.5" (OG) DIY Kit, 800 x 480
// UC8179 on the EE04 driver board, driven over SPI (write only).
//
// Full refresh: hardware reset, initialisation with the waveform from the
// panel's OTP memory, whole frame into both RAM planes, refresh (~4 s, the
// panel flashes). Partial refresh: the waveform for a high forced temperature
// (fast, no flashing), one partial window per region (~1 s each). The
// controller copies the new image into the "old" plane after every refresh,
// so a partial refresh only transfers the new data of its window.
//
// Between updates the panel voltages are switched off; the controller keeps
// its registers and RAM.

#if defined(ARDUINO)

#include <SPI.h>

#include <initializer_list>

#include "hal/display.h"

namespace hal {

class TrmnlDisplay : public Display {
 public:
  uint16_t width() const override { return kWidth; }
  uint16_t height() const override { return kHeight; }
  void show(const uint8_t* frame, size_t stride, Refresh mode, const Rect* regions,
            size_t region_count) override;

 private:
  static constexpr uint16_t kWidth = 800;
  static constexpr uint16_t kHeight = 480;

  void begin();
  void reset();
  void command(uint8_t cmd);
  void data(const uint8_t* bytes, size_t len);
  void command(uint8_t cmd, std::initializer_list<uint8_t> bytes);
  // Sends rows [y0, y1) and bytes [bx0, bx1) of the frame, inverted
  // (frame: set bit = black, panel RAM: set bit = white).
  void sendRows(const uint8_t* frame, size_t stride, int bx0, int bx1, int y0, int y1);
  bool waitWhileBusy(const char* what, uint32_t timeout_ms);
  void powerOn();
  void powerOff();
  void fullRefresh(const uint8_t* frame, size_t stride);
  void partialRefresh(const uint8_t* frame, size_t stride, const Rect& r);

  SPIClass spi_{HSPI};
  bool started_ = false;
  // True after a successful full refresh: registers are set and the "old"
  // RAM plane matches the panel, so partial refreshes are possible.
  bool ready_ = false;
};

}  // namespace hal

#endif
