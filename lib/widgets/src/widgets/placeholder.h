#pragma once

// Stand-in for a slot whose widget does not exist (yet), e.g. "weather" and
// "calendar" before they are implemented, or an empty slot.

#include <string>
#include <utility>

#include "widgets/widget.h"

namespace widgets {

class PlaceholderWidget : public Widget {
 public:
  explicit PlaceholderWidget(std::string id) : id_(std::move(id)) {}

  const char* id() const override { return id_.c_str(); }
  std::string title(ui::Language language) const override;
  SetupMessage setupMessage(ui::Language language) const override;
  bool isSetUp() const override { return false; }
  void draw(ui::Canvas&, const DrawContext&) override {}

 private:
  std::string id_;
};

}  // namespace widgets
