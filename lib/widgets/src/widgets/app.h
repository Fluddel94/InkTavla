#pragma once

// Wires everything together from the configuration: widgets for the
// header and the slots of the layout, the screen and the display controller.
// main.cpp only provides the hardware (clock, display, network) and calls
// step(); the same code runs on the PC with simulated parts.

#include <ArduinoJson.h>

#include <memory>
#include <vector>

#include "core/config.h"
#include "core/display_controller.h"
#include "hal/display.h"
#include "widgets/header/header.h"
#include "widgets/mvg/widget.h"
#include "widgets/screen.h"
#include "widgets/widget.h"

namespace widgets {

// Data sources of the widgets (network on the device, saved data in tests).
struct Services {
  mvg::Source* mvg = nullptr;
};

// Widget for an ID of layout.slots: MVG, or a placeholder for widgets that do
// not exist yet ("weather", "calendar") and unknown IDs (reported).
std::unique_ptr<Widget> createWidget(const std::string& id, JsonObjectConst root,
                                     const Services& services, core::ConfigIssues& issues);

class App {
 public:
  App(core::Clock& clock, hal::Display& display, const Services& services);

  // Builds everything from the configuration (root may be empty: defaults).
  // Sets the time zone. Call once before step().
  void configure(JsonObjectConst root, core::ConfigIssues& issues);

  core::DisplayController& controller() { return *controller_; }
  const Screen& screen() const { return *screen_; }
  const core::CoreConfig& config() const { return config_; }

  // One pass of the main loop; returns the milliseconds to wait.
  int64_t step() { return controller_->step(); }

 private:
  core::Clock& clock_;
  hal::Display& display_;
  Services services_;
  core::CoreConfig config_;
  header::HeaderWidget header_;
  std::vector<std::unique_ptr<Widget>> slots_;
  std::unique_ptr<Screen> screen_;
  std::unique_ptr<core::DisplayController> controller_;
};

}  // namespace widgets
