#pragma once

// The list of all widgets: one entry per widget with what is needed to put
// it on the screen. Adding a widget means adding its folder, one entry in
// widgetTypes() and its API object in Services and deviceServices() - all
// in registry.cpp; main.cpp and the app stay as they are.

#include <ArduinoJson.h>

#include <memory>
#include <string_view>
#include <vector>

#include "core/config.h"
#include "ui/layout.h"
#include "widgets/calendar/widget.h"
#include "widgets/mvg/widget.h"
#include "widgets/weather/widget.h"
#include "widgets/widget.h"

namespace widgets {

// Where the data sources get their data from: the network APIs on the
// device, saved responses in tests. A missing entry leaves the widget
// without data (it shows "Nicht eingerichtet").
struct Services {
  mvg::Source* mvg = nullptr;
  weather::Source* weather = nullptr;
  calendar::Source* calendar = nullptr;
};

#if defined(ARDUINO)
// The APIs of the device (created once; reads whether a calendar address is
// set).
Services deviceServices();
#endif

struct WidgetType {
  // ID in the configuration ("layout.slots", section "widgets.<id>").
  const char* id;
  // Slot sizes its view is made for (see ui::SlotSize).
  std::vector<ui::SlotSize> sizes;
  // Reads the widget's configuration section (reporting problems) and
  // creates its data source; nullptr if the service it needs is missing
  // (e.g. when only checking a configuration).
  std::unique_ptr<DataSource> (*create_source)(JsonObjectConst root, const Services& services,
                                               core::ConfigIssues& issues);
  // Creates a view of a source made by create_source for a slot of the
  // given size (one of `sizes`).
  std::unique_ptr<View> (*create_view)(const DataSource& source, ui::SlotSize size);

  bool fits(ui::SlotSize size) const;
};

const std::vector<WidgetType>& widgetTypes();

// The widget with this ID, nullptr if there is none.
const WidgetType* findWidgetType(std::string_view id);

}  // namespace widgets
