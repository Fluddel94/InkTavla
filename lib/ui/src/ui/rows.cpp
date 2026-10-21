#include "ui/rows.h"

#include <algorithm>

namespace ui {

int rowsThatFit(int height, int row_height, int blocks, int gap) {
  if (row_height <= 0 || blocks <= 0) return 0;
  return std::max(0, (height - gap * (blocks - 1)) / row_height);
}

std::vector<int> distributeRows(int total, const std::vector<int>& available) {
  const int n = static_cast<int>(available.size());
  std::vector<int> rows(n, 0);
  if (n == 0 || total <= 0) return rows;
  int left = total;
  for (int i = 0; i < n; i++) {
    const int share = total / n + (i < total % n ? 1 : 0);
    rows[i] = std::min(std::max(available[i], 1), share);
    left -= rows[i];
  }
  for (int i = 0; i < n && left > 0; i++) {
    const int more = std::min(left, std::max(available[i], 1) - rows[i]);
    rows[i] += more;
    left -= more;
  }
  return rows;
}

}  // namespace ui
