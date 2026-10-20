#include "ui/i18n.h"

namespace ui {

Language languageFromCode(std::string_view code) {
  return code == "en" ? Language::kEnglish : Language::kGerman;
}

const char* text(Language language, Text id) {
  const bool de = language == Language::kGerman;
  switch (id) {
    case Text::kNow:
      return de ? "jetzt" : "now";
    case Text::kMinutesUnit:
      return "min";
    case Text::kNoDepartures:
      return de ? "Keine Abfahrten" : "No departures";
  }
  return "";
}

}  // namespace ui
