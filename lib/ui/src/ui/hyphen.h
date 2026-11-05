#pragma once

// Hyphenation of German words (for the disruption text, whose long compound
// words ("Stellwerksstörung") would otherwise be cut off at the line end).
//
// No pattern tables: a small dictionary of railway words with their
// syllables and the usual rules for syllable breaks (a single consonant
// goes to the next syllable, of several only the last one; "ch", "sch",
// "ck", "ph", "th" and "qu" stay together, and so do "schr", "spr" and "str"
// at the start of a syllable). Breaks at word boundaries of
// compounds (dictionary) are preferred to breaks between syllables.

#include <cstddef>
#include <string_view>
#include <vector>

namespace ui {

struct HyphenPoint {
  size_t offset = 0;  // byte offset in the word where the second part starts
  // True: the break comes after a hyphen the word already has ("S-Bahn-
  // Strecke"), so no hyphen is added. False: a hyphen is added.
  bool at_hyphen = false;
  // True: a boundary between the parts of a compound word, false: a
  // syllable break.
  bool compound = false;
};

// Places where the word may be broken, in increasing order. At least two
// letters stay before and three after a break. Words that are not German
// (numbers, abbreviations) get no or arbitrary but harmless points.
std::vector<HyphenPoint> hyphenationPoints(std::string_view word);

}  // namespace ui
