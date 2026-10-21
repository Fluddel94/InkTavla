#pragma once

// Fill rule of the "Briefing" design: lists fill their column down to the
// bottom; how many rows they get follows from the free height, not from a
// fixed number.

#include <vector>

namespace ui {

// Rows of row_height that fit into height when they are split into
// `blocks` blocks with `gap` pixels between two blocks (e.g. the title of
// the next section). 0 if nothing fits.
int rowsThatFit(int height, int row_height, int blocks = 1, int gap = 0);

// Distributes `total` rows over blocks with the given numbers of entries:
// evenly (earlier blocks get the remainder), rows a block cannot fill go to
// the others. Every block keeps at least one row (e.g. for "no
// departures").
std::vector<int> distributeRows(int total, const std::vector<int>& available);

}  // namespace ui
