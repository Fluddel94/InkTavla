#include "core/update_check.h"

#include <ArduinoJson.h>

#include <cctype>
#include <cstdlib>

namespace core {

namespace {

// A number without sign and without leading zeros (Semantic Versioning).
bool parseNumber(const std::string& text, int& out) {
  if (text.empty() || text.size() > 9) return false;
  for (char c : text) {
    if (!std::isdigit(static_cast<unsigned char>(c))) return false;
  }
  if (text.size() > 1 && text[0] == '0') return false;
  out = std::atoi(text.c_str());
  return true;
}

bool isNumeric(const std::string& text) {
  if (text.empty()) return false;
  for (char c : text) {
    if (!std::isdigit(static_cast<unsigned char>(c))) return false;
  }
  return true;
}

// Dot-separated identifiers of letters, digits and "-", none empty.
bool validIdentifiers(const std::string& text) {
  if (text.empty() || text.front() == '.' || text.back() == '.') return false;
  char previous = '.';
  for (char c : text) {
    if (c == '.' && previous == '.') return false;
    if (c != '.' && c != '-' && !std::isalnum(static_cast<unsigned char>(c))) return false;
    previous = c;
  }
  return true;
}

std::string nextPart(const std::string& text, size_t& pos) {
  const size_t dot = text.find('.', pos);
  const std::string part =
      text.substr(pos, dot == std::string::npos ? std::string::npos : dot - pos);
  pos = dot == std::string::npos ? text.size() + 1 : dot + 1;
  return part;
}

std::string lower(std::string text) {
  for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return text;
}

const char* statusName(UpdateStatus status) {
  switch (status) {
    case UpdateStatus::kAvailable:
      return "available";
    case UpdateStatus::kUpToDate:
      return "up_to_date";
    case UpdateStatus::kNewerInstalled:
      return "newer_installed";
    case UpdateStatus::kNoRelease:
      return "no_release";
    case UpdateStatus::kNotFound:
      return "not_found";
    case UpdateStatus::kNoConnection:
      return "no_connection";
    case UpdateStatus::kUnexpected:
      break;
  }
  return "unexpected";
}

}  // namespace

const char* updateStatusName(UpdateStatus status) { return statusName(status); }

bool parseVersion(const std::string& text, Version& out) {
  std::string core_part = text.substr(0, text.find('+'));
  if (text.find('+') != std::string::npos && !validIdentifiers(text.substr(text.find('+') + 1))) {
    return false;
  }
  std::string pre;
  const size_t dash = core_part.find('-');
  if (dash != std::string::npos) {
    pre = core_part.substr(dash + 1);
    core_part = core_part.substr(0, dash);
    if (!validIdentifiers(pre)) return false;
    // Numeric identifiers must not have leading zeros either.
    for (size_t pos = 0; pos <= pre.size();) {
      const std::string part = nextPart(pre, pos);
      if (isNumeric(part) && part.size() > 1 && part[0] == '0') return false;
    }
  }
  Version v;
  size_t pos = 0;
  if (!parseNumber(nextPart(core_part, pos), v.major)) return false;
  if (pos > core_part.size() || !parseNumber(nextPart(core_part, pos), v.minor)) return false;
  if (pos > core_part.size() || !parseNumber(nextPart(core_part, pos), v.patch)) return false;
  if (pos <= core_part.size()) return false;  // a fourth number
  v.pre = pre;
  out = v;
  return true;
}

int compareVersions(const Version& a, const Version& b) {
  if (a.major != b.major) return a.major < b.major ? -1 : 1;
  if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
  if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
  // A pre-release comes before the release.
  if (a.pre.empty() || b.pre.empty()) {
    if (a.pre.empty() && b.pre.empty()) return 0;
    return a.pre.empty() ? 1 : -1;
  }
  size_t pa = 0, pb = 0;
  while (pa <= a.pre.size() && pb <= b.pre.size()) {
    const std::string x = nextPart(a.pre, pa);
    const std::string y = nextPart(b.pre, pb);
    const bool nx = isNumeric(x), ny = isNumeric(y);
    if (nx && ny) {
      const long ix = std::atol(x.c_str()), iy = std::atol(y.c_str());
      if (ix != iy) return ix < iy ? -1 : 1;
    } else if (nx != ny) {
      return nx ? -1 : 1;  // numbers come before words
    } else if (x != y) {
      return x < y ? -1 : 1;
    }
  }
  // More identifiers win when all before are equal.
  const bool more_a = pa <= a.pre.size(), more_b = pb <= b.pre.size();
  if (more_a == more_b) return 0;
  return more_a ? 1 : -1;
}

std::string latestReleaseUrl(const std::string& repository) {
  return "https://github.com/" + repository + "/releases/latest";
}

std::string releaseFirmwareUrl(const std::string& repository, const std::string& tag) {
  return "https://github.com/" + repository + "/releases/download/" + tag + "/" +
         kReleaseFirmwareFile;
}

std::string releaseNotesUrl(const std::string& repository, const std::string& tag) {
  return "https://github.com/" + repository + "/releases/tag/" + tag;
}

UpdateInfo evaluateLatestRelease(int http_status, const std::string& location,
                                 const std::string& repository, const std::string& installed) {
  UpdateInfo info;
  info.installed = installed;
  info.http_status = http_status;
  if (http_status <= 0) {
    info.status = UpdateStatus::kNoConnection;
    return info;
  }
  if (http_status == 404) {
    info.status = UpdateStatus::kNotFound;
    return info;
  }
  if (http_status != 301 && http_status != 302 && http_status != 303 && http_status != 307 &&
      http_status != 308) {
    info.status = UpdateStatus::kUnexpected;
    return info;
  }
  // The address may be absolute or start at the path; query and fragment
  // are left out. GitHub may write the repository in another case.
  std::string path = location.substr(0, location.find_first_of("?#"));
  const std::string host = "https://github.com";
  if (lower(path.substr(0, host.size())) == host) path = path.substr(host.size());
  const std::string base = lower("/" + repository + "/releases");
  if (lower(path.substr(0, base.size())) != base) return info;  // kUnexpected
  const std::string rest = path.substr(base.size());
  if (rest.empty() || rest == "/") {
    info.status = UpdateStatus::kNoRelease;
    return info;
  }
  const std::string prefix = "/tag/";
  if (rest.compare(0, prefix.size(), prefix) != 0) return info;
  const std::string tag = rest.substr(prefix.size());
  Version latest;
  // Only tags "vX.Y.Z[-pre]" (Grundsatz 4); that also keeps the tag safe to
  // put into the download address.
  if (tag.size() < 2 || tag[0] != 'v' || !parseVersion(tag.substr(1), latest)) return info;
  info.tag = tag;
  info.latest = tag.substr(1);
  info.notes_url = releaseNotesUrl(repository, tag);
  Version current;
  if (!parseVersion(installed, current)) {
    info.status = UpdateStatus::kAvailable;  // unknown own version: offer the release
    return info;
  }
  const int order = compareVersions(latest, current);
  info.status = order > 0    ? UpdateStatus::kAvailable
                : order == 0 ? UpdateStatus::kUpToDate
                             : UpdateStatus::kNewerInstalled;
  return info;
}

std::string updateInfoJson(const UpdateInfo& info) {
  JsonDocument doc;
  doc["status"] = statusName(info.status);
  doc["installed"] = info.installed;
  if (!info.latest.empty()) {
    doc["latest"] = info.latest;
    doc["notes_url"] = info.notes_url;
  }
  if (info.status == UpdateStatus::kUnexpected) doc["http_status"] = info.http_status;
  std::string out;
  serializeJson(doc, out);
  return out;
}

}  // namespace core
