#include "ui/hyphen.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include "ui/text.h"

namespace ui {

namespace {

constexpr size_t kMinBefore = 2;  // letters before a break
constexpr size_t kMinAfter = 3;   // letters after a break

// Words and word parts of the disruption messages, written with their
// syllable breaks ("-"), lower case. A match inside a longer word gives a
// compound boundary at its start and end and the breaks inside it.
constexpr const char* kStems[] = {
    "stell-werk",      "stö-rung",
    "ver-spä-tung",    "ver-zö-ge-rung",
    "bau-ar-bei-ten",  "bau-stel-le",
    "ober-lei-tung",   "fahr-lei-tung",
    "bahn-über-gang",  "bahn-hof",
    "bahn-steig",      "bahn-damm",
    "sig-nal",         "wei-che",
    "fahr-zeug",       "fahr-gäs-te",
    "ver-kehr",        "ein-satz",
    "po-li-zei",       "ret-tungs-dienst",
    "feu-er-wehr",     "not-arzt",
    "er-satz",         "sper-rung",
    "aus-fall",        "aus-fäl-le",
    "de-fekt",         "tech-nisch",
    "stre-cke",        "un-fall",
    "per-so-nen",      "scha-den",
    "ge-wit-ter",      "sturm",
    "ober-bay-ern",    "strom-ver-sor-gung",
    "strom-aus-fall",  "zug-aus-fall",
    "zug-aus-fäl-le",  "flug-ha-fen",
    "haupt-bahn-hof",  "ost-bahn-hof",
    "nach-rich-ten",   "um-lei-tung",
    "ein-schrän-kung", "ein-schrän-kun-gen",
    "hin-der-nis",     "bahn-über-gän-ge",
};

std::u32string toCodepoints(std::string_view text, std::vector<size_t>* offsets) {
  std::u32string result;
  size_t pos = 0;
  while (pos < text.size()) {
    if (offsets != nullptr) offsets->push_back(pos);
    result.push_back(decodeUtf8(text, pos));
  }
  if (offsets != nullptr) offsets->push_back(text.size());
  return result;
}

char32_t lower(char32_t c) {
  if (c >= 'A' && c <= 'Z') return c + 32;
  if (c == 0xC4 || c == 0xD6 || c == 0xDC) return c + 32;  // Ä Ö Ü
  return c;
}

bool isVowel(char32_t c) {
  switch (c) {
    case 'a':
    case 'e':
    case 'i':
    case 'o':
    case 'u':
    case 'y':
    case 0xE4:  // ä
    case 0xF6:  // ö
    case 0xFC:  // ü
      return true;
    default:
      return false;
  }
}

bool isLetter(char32_t c) { return isVowel(c) || (c >= 'a' && c <= 'z') || c == 0xDF; }

bool isDiphthong(char32_t a, char32_t b) {
  return (a == 'e' && b == 'i') || (a == 'i' && b == 'e') || (a == 'a' && b == 'u') ||
         (a == 'e' && b == 'u') || (a == 0xE4 && b == 'u') || (a == 'a' && b == 'i') ||
         (a == 'e' && b == 'y') || (a == 'a' && b == 'y') || (a == 'o' && b == 'i');
}

// Length of the inseparable consonant group at i ("sch", "ch", "ck", ...), 1
// for a single consonant.
size_t consonantUnit(const std::u32string& w, size_t i, size_t end) {
  const auto at = [&](size_t k) { return i + k < end ? w[i + k] : U'\0'; };
  if (at(0) == 's' && at(1) == 'c' && at(2) == 'h') return 3;
  if (at(0) == 'c' && (at(1) == 'h' || at(1) == 'k')) return 2;
  if ((at(0) == 'p' || at(0) == 't') && at(1) == 'h') return 2;
  if (at(0) == 'q' && at(1) == 'u') return 2;
  return 1;
}

// The consonants [from, to) between two vowel groups and the syllable break
// inside them.
struct Gap {
  size_t from = 0;
  size_t to = 0;
  size_t brk = 0;
};

// Syllable breaks of a run of letters [begin, end): positions where the
// second syllable starts.
void syllableBreaks(const std::u32string& w, size_t begin, size_t end, std::vector<Gap>& out) {
  // Vowel groups ("nuclei"): [start, stop).
  std::vector<std::pair<size_t, size_t>> nuclei;
  for (size_t i = begin; i < end;) {
    if (!isVowel(w[i])) {
      i++;
      continue;
    }
    size_t stop = i + 1;
    if (stop < end && isDiphthong(w[i], w[stop])) stop++;
    nuclei.emplace_back(i, stop);
    i = stop;
  }
  for (size_t k = 0; k + 1 < nuclei.size(); k++) {
    const size_t from = nuclei[k].second;
    const size_t to = nuclei[k + 1].first;
    if (from == to) {  // two vowels that are no diphthong: "Ide-en"
      out.push_back({from, to, from});
      continue;
    }
    // Consonant units between the nuclei; the last one starts the next
    // syllable.
    size_t last = from;
    for (size_t i = from; i < to;) {
      last = i;
      i += consonantUnit(w, i, to);
    }
    out.push_back({from, to, last});
  }
}

}  // namespace

std::vector<HyphenPoint> hyphenationPoints(std::string_view word) {
  std::vector<size_t> offsets;
  const std::u32string raw = toCodepoints(word, &offsets);
  std::u32string w = raw;
  for (char32_t& c : w) c = lower(c);
  const size_t n = w.size();

  // 0 = no break, 1 = syllable break, 2 = compound boundary, 3 = after a hyphen
  std::vector<uint8_t> kind(n + 1, 0);
  std::vector<bool> from_dictionary(n + 1, false);    // syllable breaks of a stem
  std::vector<std::pair<size_t, size_t>> stem_spans;  // where a stem was found
  const auto mark = [&](size_t pos, uint8_t k) {
    if (pos >= kMinBefore && pos + kMinAfter <= n && k > kind[pos]) kind[pos] = k;
  };

  // Runs of letters between hyphens and other characters.
  std::vector<Gap> gaps;
  for (size_t i = 0; i < n;) {
    if (!isLetter(w[i])) {
      if (w[i] == '-' && i > 0 && i + 1 < n && isLetter(w[i - 1]) && isLetter(w[i + 1])) {
        mark(i + 1, 3);
      }
      i++;
      continue;
    }
    size_t end = i;
    while (end < n && isLetter(w[end])) end++;
    const size_t first_gap = gaps.size();
    syllableBreaks(w, i, end, gaps);
    for (size_t g = first_gap; g < gaps.size(); g++) mark(gaps[g].brk, 1);
    i = end;
  }

  // Dictionary: every stem found in the word gives compound boundaries at
  // its start and end, and its own syllable breaks. A linking "s" ("Stell-
  // werks|störung") stays with the first part.
  for (const char* stem : kStems) {
    std::u32string plain;
    std::vector<size_t> inner;
    for (const char* p = stem; *p != '\0';) {
      std::string_view rest(p);
      size_t pos = 0;
      const char32_t c = decodeUtf8(rest, pos);
      p += pos;
      if (c == '-') {
        inner.push_back(plain.size());
      } else {
        plain.push_back(c);
      }
    }
    for (size_t at = w.find(plain); at != std::u32string::npos; at = w.find(plain, at + 1)) {
      const size_t stop = at + plain.size();
      stem_spans.emplace_back(at, stop);
      if (plain.size() >= 4) {
        mark(at, 2);
        mark(stop < n && w[stop] == 's' && stop + 1 < n ? stop + 1 : stop, 2);
      }
      for (size_t b : inner) {
        mark(at + b, 1);
        if (kind[at + b] != 0) from_dictionary[at + b] = true;
      }
    }
  }

  // Inside a stem only the breaks of the dictionary count ("Bahn|über|gang",
  // not "Bahn|ü|ber|gang").
  for (const auto& span : stem_spans) {
    for (size_t pos = span.first + 1; pos < span.second && pos <= n; pos++) {
      if (kind[pos] == 1 && !from_dictionary[pos]) kind[pos] = 0;
    }
  }

  // Where a compound boundary lies between two vowel groups, the syllable
  // break of the same gap is dropped ("Stellwerks|störung", not
  // "Stellwerkss|törung").
  for (const Gap& g : gaps) {
    for (size_t pos = g.from; pos <= g.to && pos <= n; pos++) {
      if ((kind[pos] >= 2 || from_dictionary[pos]) && pos != g.brk) {
        if (kind[g.brk] == 1) kind[g.brk] = 0;
        break;
      }
    }
  }

  std::vector<HyphenPoint> result;
  for (size_t pos = 0; pos <= n; pos++) {
    if (kind[pos] == 0) continue;
    HyphenPoint p;
    p.offset = offsets[pos];
    p.at_hyphen = kind[pos] == 3;
    p.compound = kind[pos] >= 2;
    result.push_back(p);
  }
  return result;
}

}  // namespace ui
