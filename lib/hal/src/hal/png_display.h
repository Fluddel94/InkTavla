#pragma once

// Simulated panel for the PC: keeps the image the panel would show and
// writes it as PNG after every update. Used to run the complete main loop
// without hardware (tests, previews).

#if !defined(ARDUINO)

#include <cstdint>
#include <string>
#include <vector>

#include "hal/display.h"

namespace hal {

class PngDisplay : public Display {
 public:
  // One call of show(), for checks in tests.
  struct Update {
    Refresh mode;
    std::vector<Rect> regions;  // empty for kFull
  };

  // Writes <directory>/<prefix>NNNN.png after every update ("" = no files).
  PngDisplay(uint16_t width, uint16_t height, std::string directory = "",
             std::string prefix = "frame_");

  uint16_t width() const override { return width_; }
  uint16_t height() const override { return height_; }
  void show(const uint8_t* frame, size_t stride, Refresh mode, const Rect* regions,
            size_t region_count) override;

  // Image the panel shows now: only refreshed regions are taken over.
  const std::vector<uint8_t>& panel() const { return panel_; }
  size_t stride() const { return stride_; }
  const std::vector<Update>& updates() const { return updates_; }
  std::string png() const;

 private:
  uint16_t width_;
  uint16_t height_;
  size_t stride_;
  std::string directory_;
  std::string prefix_;
  std::vector<uint8_t> panel_;
  std::vector<Update> updates_;
};

}  // namespace hal

#endif
