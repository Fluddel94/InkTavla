#pragma once

// QR codes for the setup screen and the settings notice, generated with
// Nayuki's QR Code generator library (lib/qrcodegen, MIT).

#include <cstdint>
#include <string_view>
#include <vector>

#include "ui/canvas.h"

namespace ui {

class QrCode {
 public:
  // Encodes the text (byte mode, error correction "medium", raised if the
  // version allows). Empty (size() == 0) if the text is too long.
  explicit QrCode(std::string_view text);

  // Modules per side (21 for version 1, +4 per version); 0 if invalid.
  int size() const { return size_; }
  bool module(int x, int y) const { return modules_[y * size_ + x] != 0; }

 private:
  int size_ = 0;
  std::vector<uint8_t> modules_;
};

// Side length in pixels of the QR code with a quiet zone of `quiet`
// modules on every side.
int qrPixelSize(const QrCode& code, int module_px, int quiet = 2);

// Draws the code with its top-left corner (including the white quiet zone)
// at (x, y). Returns the side length in pixels.
int drawQrCode(Canvas& canvas, int x, int y, const QrCode& code, int module_px, int quiet = 2);

// Largest module size so that the code (with quiet zone) fits into
// max_px; at least 1.
int qrModuleFor(const QrCode& code, int max_px, int quiet = 2);

}  // namespace ui
