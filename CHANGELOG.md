# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- PlatformIO project for the XIAO ESP32-S3 Plus (pioarduino 55.03.312-1,
  Arduino-ESP32 3.3.12, board `seeed_xiao_esp32_s3_plus`).
- 16 MB partition table: NVS, two 4 MB OTA app slots, ~7.9 MB LittleFS,
  core dump.
- Layer skeleton (HAL, core, UI, widgets) with minimal interfaces.
- Host unit tests (`pio test -e native`) with MVG API responses as fixtures.
