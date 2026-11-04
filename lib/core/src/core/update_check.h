#pragma once

// Looking for a newer firmware on GitHub, only when the button on the
// settings page is pressed (never on its own).
//
// No GitHub API is used (it has request limits and needs JSON parsing):
// https://github.com/<repository>/releases/latest answers with a redirect to
// the page of the newest release, .../releases/tag/v1.2.3, and the version
// is read from that address. Pre-releases are never "latest". Without any
// release GitHub redirects to .../releases; a private (or missing)
// repository answers 404.
//
// The firmware is then downloaded from that exact tag,
// .../releases/download/v1.2.3/firmware.bin (so the version shown is the
// one installed even if a new release appears in between), and written the
// same way as an uploaded file (core/firmware_writer.h).
//
// GitHub sees the address (IP) of the network when the button is pressed;
// nothing else is sent (Grundsatz 12).

#include <string>

namespace core {

// The repository whose releases are offered. A fork can set its own with
// -DINKTAVLA_UPDATE_REPOSITORY=\"owner/name\" in build_flags (platformio.ini).
#ifdef INKTAVLA_UPDATE_REPOSITORY
inline constexpr char kUpdateRepository[] = INKTAVLA_UPDATE_REPOSITORY;
#else
inline constexpr char kUpdateRepository[] = "Fluddel94/InkTavla";
#endif

// The file of a release that the device installs (Grundsatz 4).
inline constexpr char kReleaseFirmwareFile[] = "firmware.bin";

// MAJOR.MINOR.PATCH with an optional pre-release ("-dev", "-rc.1"); build
// metadata ("+abc") is accepted and ignored, as Semantic Versioning says.
struct Version {
  int major = 0;
  int minor = 0;
  int patch = 0;
  std::string pre;  // without the "-", empty for a release
};

// False for anything that is not such a version (also a leading "v").
bool parseVersion(const std::string& text, Version& out);

// <0, 0 or >0 by Semantic Versioning precedence: 1.0.0-dev < 1.0.0-rc.1 <
// 1.0.0 < 1.0.1; numeric pre-release parts compare as numbers.
int compareVersions(const Version& a, const Version& b);

std::string latestReleaseUrl(const std::string& repository);
std::string releaseFirmwareUrl(const std::string& repository, const std::string& tag);
std::string releaseNotesUrl(const std::string& repository, const std::string& tag);

enum class UpdateStatus {
  kAvailable,       // a newer release than the installed firmware
  kUpToDate,        // the installed firmware is the newest release
  kNewerInstalled,  // installed is newer (a development build): nothing offered
  kNoRelease,       // the repository has no release yet
  kNotFound,        // 404: repository private or missing
  kNoConnection,    // no answer (no Wi-Fi, no internet, timeout)
  kUnexpected,      // any other answer
};

struct UpdateInfo {
  UpdateStatus status = UpdateStatus::kUnexpected;
  std::string installed;  // kFirmwareVersion
  std::string latest;     // e.g. "0.2.0" (when a release was found)
  std::string tag;        // e.g. "v0.2.0"
  std::string notes_url;  // page of that release
  int http_status = 0;    // for kUnexpected
};

// Evaluates the answer to GET latestReleaseUrl(repository) without
// following the redirect: http_status as from the HTTP client (negative:
// no connection), location the value of its Location header.
UpdateInfo evaluateLatestRelease(int http_status, const std::string& location,
                                 const std::string& repository, const std::string& installed);

// For the settings page: {"status": "available"|..., "installed", "latest",
// "notes_url", "http_status"}.
std::string updateInfoJson(const UpdateInfo& info);

}  // namespace core
