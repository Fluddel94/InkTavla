# Releasing

How a version of InkTavla is released. Every release follows the same
steps; the update button and the web installer rely on them (tag name,
file names), so they are not changed without a reason.

## Versions

- Versions follow [Semantic Versioning](https://semver.org/):
  `MAJOR.MINOR.PATCH`. While the version is `0.x`, new functions or changed
  settings raise MINOR (`0.2.0`), fixes only raise PATCH (`0.1.1`).
  `1.0.0` is a deliberate milestone.
- The version of the firmware is `kFirmwareVersion` in
  `lib/core/src/core/version.h`. Between releases it is the next MINOR
  version with `-dev` (after `0.2.0`: `0.3.0-dev`), so a development build
  never shows a version that was already released. The update button
  compares versions by Semantic Versioning: `0.3.0-dev` is newer than
  `0.2.0` and older than `0.3.0`.
- [CHANGELOG.md](../CHANGELOG.md) follows
  [Keep a Changelog](https://keepachangelog.com/) and is kept by hand.
  Entries describe what changed for users since the last release, under
  `## [Unreleased]` with `Added`, `Changed`, `Fixed`, `Removed`; steps in
  between that came and went are not listed.

## Steps

1. **Release commit.** In one commit and nothing else:
   - `version.h`: `kFirmwareVersion` set to `X.Y.Z` (no `-dev`).
   - `CHANGELOG.md`: `## [Unreleased]` becomes `## [X.Y.Z] - YYYY-MM-DD`
     (the date of the release tag), with a new empty `## [Unreleased]`
     above it. Check the section for duplicates and order; a short
     introduction above the first heading is fine.
   - Native tests and the device build pass (`pio test -e native`,
     `pio run -e xiao_esp32s3_plus`).
   - Commit message exactly: `chore(release): X.Y.Z`.
2. **Firmware files.** Build exactly this commit after pushing it:
   `pio run -e xiao_esp32s3_plus` writes `firmware.bin` and
   `firmware-full.bin` to `.pio/build/xiao_esp32s3_plus/`. Alternatively
   take both files from the CI run of this commit (Actions → run →
   artifact `firmware-<commit>`); CI must be green.
3. **GitHub release** (Releases → Draft a new release):
   - **Tag:** `vX.Y.Z` (lower-case `v`, target `main`), created when the
     release is published. Tags are never moved or deleted; a mistake is
     fixed with a new PATCH version. The update button only recognises
     tags of exactly this form.
   - **Title:** the tag, `vX.Y.Z`, nothing else.
   - **Text:** the section of this version from `CHANGELOG.md`, unchanged,
     without its heading `## [X.Y.Z] - …`.
   - **Files:** exactly two, names unchanged: `firmware.bin` (the app,
     downloaded by the update button) and `firmware-full.bin` (the whole
     flash image, taken by the web installer workflow). See
     [Files of a build and of a release](updating.md#files-of-a-build-and-of-a-release).
   - **Set as the latest release:** ticked. **Set as a pre-release:** only
     for versions with a suffix (`vX.Y.Z-rc.1`), not for normal `0.x`
     versions. The update button only offers the latest release, and the
     web installer workflow skips pre-releases.
   - After publishing, the workflow "Web installer"
     (`.github/workflows/pages.yml`) runs by itself and publishes the
     installer with this version on GitHub Pages (only once the
     repository is public and Pages is set up).
4. **Next development version.** The first commit after the release sets
   `kFirmwareVersion` to `X.(Y+1).0-dev`.
