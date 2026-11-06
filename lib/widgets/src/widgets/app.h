#pragma once

// Wires everything together from the configuration: the data sources and
// views of the widgets in the layout's columns (see widgets/registry.h), the
// band, the screen and the display controller. main.cpp only provides the
// hardware (clock, display, network) and calls step(); the same code runs on
// the PC with simulated parts.

#include <ArduinoJson.h>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "core/config.h"
#include "core/display_controller.h"
#include "core/system_report.h"
#include "hal/display.h"
#include "widgets/registry.h"
#include "widgets/screen.h"
#include "widgets/widget.h"

namespace widgets {

// Reports all problems of a configuration (core settings, the sections of
// the widgets in the layout, unknown widgets and widgets in columns of a
// size they are not made for), e.g. before it is saved.
void validateConfig(JsonObjectConst root, core::ConfigIssues& issues);

class App {
 public:
  App(core::Clock& clock, hal::Display& display, const Services& services);

  // Builds everything from the configuration (root may be empty: defaults).
  // Sets the time zone. Call once before step().
  void configure(JsonObjectConst root, core::ConfigIssues& issues);

  core::DisplayController& controller() { return *controller_; }
  const Screen& screen() const { return *screen_; }
  const core::CoreConfig& config() const { return config_; }
  // Data sources in the order of the controller's contents (the order the
  // widgets first appear in layout.slots).
  const std::vector<std::unique_ptr<DataSource>>& sources() const { return sources_; }

  // Firmware version and name of the saved Wi-Fi for the start screen
  // (after configure()).
  void setStartInfo(briefing::StartInfo info) { screen_->setStartInfo(std::move(info)); }

  // What only the hardware knows, for the system page and the settings
  // page (main.cpp; fixed values in the PC simulation).
  void setDeviceInfo(std::function<core::DeviceInfo()> info) { device_info_ = std::move(info); }

  // The state of the device for the system page (KEY2) and the settings
  // page: device info, the sources of the controller, counts of the log.
  core::SystemReport systemReport() const;

  // One pass of the main loop; returns the milliseconds to wait.
  int64_t step() { return controller_->step(); }

 private:
  core::Clock& clock_;
  hal::Display& display_;
  Services services_;
  core::CoreConfig config_;
  std::vector<std::unique_ptr<DataSource>> sources_;
  std::vector<std::unique_ptr<View>> views_;
  std::unique_ptr<Screen> screen_;
  std::unique_ptr<core::DisplayController> controller_;
  std::function<core::DeviceInfo()> device_info_;
};

}  // namespace widgets
