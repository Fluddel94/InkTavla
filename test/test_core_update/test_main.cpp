// Update check on the settings page: version order, the redirect of
// GitHub's releases/latest, error cases (core/update_check.h).

#include <unity.h>

#include <string>

#include "core/update_check.h"

void setUp() {}
void tearDown() {}

namespace {

int order(const char* a, const char* b) {
  core::Version va, vb;
  TEST_ASSERT_TRUE_MESSAGE(core::parseVersion(a, va), a);
  TEST_ASSERT_TRUE_MESSAGE(core::parseVersion(b, vb), b);
  const int r = core::compareVersions(va, vb);
  return r < 0 ? -1 : r > 0 ? 1 : 0;
}

const char kRepo[] = "owner/Project";

core::UpdateInfo latest(int status, const char* location, const char* installed = "0.1.0") {
  return core::evaluateLatestRelease(status, location, kRepo, installed);
}

}  // namespace

void test_versions_are_parsed() {
  core::Version v;
  TEST_ASSERT_TRUE(core::parseVersion("0.2.0-dev", v));
  TEST_ASSERT_EQUAL_INT(0, v.major);
  TEST_ASSERT_EQUAL_INT(2, v.minor);
  TEST_ASSERT_EQUAL_INT(0, v.patch);
  TEST_ASSERT_EQUAL_STRING("dev", v.pre.c_str());
  TEST_ASSERT_TRUE(core::parseVersion("10.20.30-rc.1+build.5", v));
  TEST_ASSERT_EQUAL_INT(30, v.patch);
  TEST_ASSERT_EQUAL_STRING("rc.1", v.pre.c_str());

  const char* bad[] = {"",       "1",      "1.2",     "1.2.3.4",    "v1.2.3",
                       "01.2.3", "1.2.3-", "1.2.3-.", "1.2.3-a..b", "1.2.3-01",
                       "1.2.x",  "1.2.3+", " 1.2.3",  "1.-2.3"};
  for (const char* text : bad) TEST_ASSERT_FALSE_MESSAGE(core::parseVersion(text, v), text);
}

void test_versions_are_ordered_like_semantic_versioning() {
  TEST_ASSERT_EQUAL_INT(0, order("1.2.3", "1.2.3"));
  TEST_ASSERT_EQUAL_INT(0, order("1.2.3+a", "1.2.3+b"));
  TEST_ASSERT_EQUAL_INT(-1, order("0.1.0", "0.2.0"));
  TEST_ASSERT_EQUAL_INT(-1, order("0.9.0", "0.10.0"));  // numbers, not text
  TEST_ASSERT_EQUAL_INT(-1, order("0.1.9", "0.1.10"));
  TEST_ASSERT_EQUAL_INT(-1, order("1.9.9", "2.0.0"));
  // A development build is newer than the last release and older than its own.
  TEST_ASSERT_EQUAL_INT(1, order("0.2.0-dev", "0.1.0"));
  TEST_ASSERT_EQUAL_INT(-1, order("0.2.0-dev", "0.2.0"));
  TEST_ASSERT_EQUAL_INT(-1, order("0.2.0-dev", "0.2.0-rc.1"));
  // The example order of semver.org.
  const char* chain[] = {"1.0.0-alpha",  "1.0.0-alpha.1", "1.0.0-alpha.beta", "1.0.0-beta",
                         "1.0.0-beta.2", "1.0.0-beta.11", "1.0.0-rc.1",       "1.0.0"};
  for (size_t i = 0; i + 1 < sizeof(chain) / sizeof(chain[0]); i++) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, order(chain[i], chain[i + 1]), chain[i]);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, order(chain[i + 1], chain[i]), chain[i]);
  }
}

void test_addresses_of_a_release() {
  TEST_ASSERT_EQUAL_STRING("https://github.com/owner/Project/releases/latest",
                           core::latestReleaseUrl(kRepo).c_str());
  TEST_ASSERT_EQUAL_STRING("https://github.com/owner/Project/releases/download/v0.2.0/firmware.bin",
                           core::releaseFirmwareUrl(kRepo, "v0.2.0").c_str());
  TEST_ASSERT_EQUAL_STRING("https://github.com/owner/Project/releases/tag/v0.2.0",
                           core::releaseNotesUrl(kRepo, "v0.2.0").c_str());
}

void test_a_newer_release_is_offered() {
  const core::UpdateInfo info =
      latest(302, "https://github.com/owner/Project/releases/tag/v0.2.0", "0.1.0");
  TEST_ASSERT_TRUE(info.status == core::UpdateStatus::kAvailable);
  TEST_ASSERT_EQUAL_STRING("0.2.0", info.latest.c_str());
  TEST_ASSERT_EQUAL_STRING("v0.2.0", info.tag.c_str());
  TEST_ASSERT_EQUAL_STRING("https://github.com/owner/Project/releases/tag/v0.2.0",
                           info.notes_url.c_str());
  TEST_ASSERT_EQUAL_STRING(
      "{\"status\":\"available\",\"installed\":\"0.1.0\",\"latest\":\"0.2.0\","
      "\"notes_url\":\"https://github.com/owner/Project/releases/tag/v0.2.0\"}",
      core::updateInfoJson(info).c_str());
  // Relative address, other case of the repository, query: the same.
  TEST_ASSERT_TRUE(latest(302, "/Owner/project/releases/tag/v0.2.0?x=1").status ==
                   core::UpdateStatus::kAvailable);
  // A release candidate is newer than a development build of the same version.
  TEST_ASSERT_TRUE(
      latest(302, "https://github.com/owner/Project/releases/tag/v0.2.0-rc.1", "0.2.0-dev")
          .status == core::UpdateStatus::kAvailable);
}

void test_nothing_is_offered_for_the_same_or_an_older_release() {
  core::UpdateInfo same = latest(302, "https://github.com/owner/Project/releases/tag/v0.1.0");
  TEST_ASSERT_TRUE(same.status == core::UpdateStatus::kUpToDate);
  TEST_ASSERT_EQUAL_STRING("0.1.0", same.latest.c_str());
  // A development build (0.2.0-dev) is newer than the last release.
  core::UpdateInfo dev =
      latest(302, "https://github.com/owner/Project/releases/tag/v0.1.0", "0.2.0-dev");
  TEST_ASSERT_TRUE(dev.status == core::UpdateStatus::kNewerInstalled);
  TEST_ASSERT_EQUAL_STRING(
      "{\"status\":\"newer_installed\",\"installed\":\"0.2.0-dev\",\"latest\":\"0.1.0\","
      "\"notes_url\":\"https://github.com/owner/Project/releases/tag/v0.1.0\"}",
      core::updateInfoJson(dev).c_str());
}

void test_error_cases() {
  // Private or missing repository.
  core::UpdateInfo missing = latest(404, "");
  TEST_ASSERT_TRUE(missing.status == core::UpdateStatus::kNotFound);
  TEST_ASSERT_EQUAL_STRING("{\"status\":\"not_found\",\"installed\":\"0.1.0\"}",
                           core::updateInfoJson(missing).c_str());
  // No release yet: GitHub redirects to the list of releases.
  TEST_ASSERT_TRUE(latest(302, "https://github.com/owner/Project/releases").status ==
                   core::UpdateStatus::kNoRelease);
  TEST_ASSERT_TRUE(latest(302, "https://github.com/owner/Project/releases/").status ==
                   core::UpdateStatus::kNoRelease);
  // No connection (negative codes of the HTTP client) or no answer.
  TEST_ASSERT_TRUE(latest(-1, "").status == core::UpdateStatus::kNoConnection);
  TEST_ASSERT_TRUE(latest(-11, "").status == core::UpdateStatus::kNoConnection);
  TEST_ASSERT_TRUE(latest(0, "").status == core::UpdateStatus::kNoConnection);
  // Anything else is unexpected and offers nothing.
  core::UpdateInfo server = latest(500, "");
  TEST_ASSERT_TRUE(server.status == core::UpdateStatus::kUnexpected);
  TEST_ASSERT_EQUAL_STRING(
      "{\"status\":\"unexpected\",\"installed\":\"0.1.0\",\"http_status\":500}",
      core::updateInfoJson(server).c_str());
  const char* odd[] = {
      "",                                                           // redirect without address
      "https://github.com/other/Project/releases/tag/v9.0.0",       // another repository
      "https://example.com/owner/Project/releases/tag/v9.0.0",      // another host
      "https://github.com/owner/Project/releases/tag/9.0.0",        // tag without "v"
      "https://github.com/owner/Project/releases/tag/v9.0",         // no version
      "https://github.com/owner/Project/releases/tag/v9.0.0/../x",  // not just a tag
      "https://github.com/owner/Project/releases/tag/v9.0.0%2F..",  // encoded characters
      "https://github.com/owner/Project/tree/main",                 // something else
      "https://github.com/login?return_to=x",                       // sign-in page
  };
  for (const char* location : odd) {
    const core::UpdateInfo info = latest(302, location);
    TEST_ASSERT_TRUE_MESSAGE(info.status == core::UpdateStatus::kUnexpected, location);
    TEST_ASSERT_TRUE_MESSAGE(info.tag.empty(), location);
  }
  // 200 (no redirect) is unexpected as well.
  TEST_ASSERT_TRUE(latest(200, "").status == core::UpdateStatus::kUnexpected);
}

void test_an_unknown_own_version_still_gets_the_release() {
  TEST_ASSERT_TRUE(
      latest(302, "https://github.com/owner/Project/releases/tag/v0.1.0", "unknown").status ==
      core::UpdateStatus::kAvailable);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_versions_are_parsed);
  RUN_TEST(test_versions_are_ordered_like_semantic_versioning);
  RUN_TEST(test_addresses_of_a_release);
  RUN_TEST(test_a_newer_release_is_offered);
  RUN_TEST(test_nothing_is_offered_for_the_same_or_an_older_release);
  RUN_TEST(test_error_cases);
  RUN_TEST(test_an_unknown_own_version_still_gets_the_release);
  return UNITY_END();
}
