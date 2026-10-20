#pragma once

#include <string_view>

namespace ui {

// Language of texts on the display (configuration setting "language").
enum class Language { kGerman, kEnglish };

// "de" -> kGerman, "en" -> kEnglish; anything else kGerman.
Language languageFromCode(std::string_view code);

enum class Text {
  kNow,           // departure in this minute
  kMinutesUnit,   // after a number of minutes: "4 min"
  kNoDepartures,  // empty departure list
};

const char* text(Language language, Text id);

}  // namespace ui
