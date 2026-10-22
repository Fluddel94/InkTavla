#pragma once

// Stand-in for a slot without a working view: a widget that does not exist
// (yet), a widget on a slot of a size it is not made for, or an empty slot.
// It shows the tile title and a message.

#include <string>
#include <utility>

#include "ui/i18n.h"
#include "widgets/widget.h"

namespace widgets {

class PlaceholderView : public View {
 public:
  // id: the widget ID from the configuration (gives the title); message and
  // detail: the texts shown.
  // column_style: like the columns of "briefing" instead of a tile.
  PlaceholderView(std::string id, ui::Text message, std::string detail, bool column_style = false)
      : id_(std::move(id)),
        message_(message),
        detail_(std::move(detail)),
        column_style_(column_style) {}

  // "Nicht eingerichtet" / "Noch nicht verfügbar" for an unknown widget.
  static PlaceholderView notAvailable(std::string id, bool column_style = false);
  // "Passt hier nicht" / "config.json: layout.slots" for a widget on a slot
  // of a size it is not made for.
  static PlaceholderView doesNotFit(std::string id, bool column_style = false);

  std::string title(ui::Language language) const override;
  void draw(ui::Canvas& canvas, const DrawContext& context) override;

 private:
  std::string id_;
  ui::Text message_;
  std::string detail_;  // "" = the text kNotAvailable in the language
  bool column_style_;
};

}  // namespace widgets
