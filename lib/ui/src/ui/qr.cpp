#include "ui/qr.h"

#include <algorithm>
#include <string>

#include "qrcodegen.h"

namespace ui {

QrCode::QrCode(std::string_view text) {
  // Buffers for the largest version the screen could show sensibly.
  constexpr int kMaxVersion = 10;
  std::vector<uint8_t> temp(qrcodegen_BUFFER_LEN_FOR_VERSION(kMaxVersion));
  std::vector<uint8_t> code(qrcodegen_BUFFER_LEN_FOR_VERSION(kMaxVersion));
  if (text.size() > temp.size()) return;
  std::copy(text.begin(), text.end(), temp.begin());
  if (!qrcodegen_encodeBinary(temp.data(), text.size(), code.data(), qrcodegen_Ecc_MEDIUM,
                              qrcodegen_VERSION_MIN, kMaxVersion, qrcodegen_Mask_AUTO, true)) {
    return;
  }
  size_ = qrcodegen_getSize(code.data());
  modules_.resize(static_cast<size_t>(size_) * size_);
  for (int y = 0; y < size_; y++) {
    for (int x = 0; x < size_; x++) {
      modules_[y * size_ + x] = qrcodegen_getModule(code.data(), x, y) ? 1 : 0;
    }
  }
}

int qrPixelSize(const QrCode& code, int module_px, int quiet) {
  return (code.size() + 2 * quiet) * module_px;
}

int drawQrCode(Canvas& canvas, int x, int y, const QrCode& code, int module_px, int quiet) {
  const int side = qrPixelSize(code, module_px, quiet);
  canvas.fillRect(x, y, side, side, Color::kWhite);
  const int x0 = x + quiet * module_px;
  const int y0 = y + quiet * module_px;
  for (int my = 0; my < code.size(); my++) {
    for (int mx = 0; mx < code.size(); mx++) {
      if (code.module(mx, my)) {
        canvas.fillRect(x0 + mx * module_px, y0 + my * module_px, module_px, module_px,
                        Color::kBlack);
      }
    }
  }
  return side;
}

int qrModuleFor(const QrCode& code, int max_px, int quiet) {
  const int modules = code.size() + 2 * quiet;
  return modules <= 0 ? 1 : std::max(1, max_px / modules);
}

}  // namespace ui
