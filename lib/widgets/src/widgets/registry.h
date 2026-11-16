#pragma once

// The list of all widgets: one entry per widget with what is needed to put
// it on the screen. Adding a widget means adding its folder, one entry in
// widgetTypes() and its API object in Services and deviceServices() - all
// in registry.cpp; main.cpp and the app stay as they are.

#include <ArduinoJson.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/config.h"
#if defined(ARDUINO)
#include "core/settings_server.h"
#endif
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

// What the firmware knows about a widget: its description (ID, smallest
// size step, what it offers for the band) and how its source and view are
// made. The description is kept in this one place so that it can later be
// generated from a file per widget.
struct WidgetType {
  // ID in the configuration ("layout.slots", section "widgets.<id>").
  const char* id;
  // Smallest size step its view is made for (see ui::SizeStep). The view
  // has to draw at every step from this one on and stretch in between;
  // showing more at a wider step is up to the widget.
  ui::SizeStep min_step;
  // Whether it has information for the band (sentence, picture), and
  // whether the band shows it even if the widget is in no column (its data
  // is fetched then, too).
  bool band;
  bool band_alone;
  // Reads the widget's configuration section (reporting problems) and
  // creates its data source; nullptr if the service it needs is missing
  // (e.g. when only checking a configuration).
  std::unique_ptr<DataSource> (*create_source)(JsonObjectConst root, const Services& services,
                                               core::ConfigIssues& issues);
  // Creates a view of a source made by create_source for a column of the
  // given step (min_step or wider); the canvas has the exact width.
  std::unique_ptr<View> (*create_view)(const DataSource& source, ui::SizeStep step);

  bool fits(ui::SizeStep step) const { return step >= min_step; }
};

const std::vector<WidgetType>& widgetTypes();

// The layout for the widgets of "layout.slots": two entries give band + 2,
// everything else band + 3. In band + 3 the wide column (M) is the first
// one from the left whose widget needs M, otherwise the first column.
ui::LayoutOptions layoutOptions(const std::vector<std::string>& slots, bool dividers = true);

// For the settings page, as JSON object members: the widgets with their
// smallest step and whether they offer something for the band, the layouts
// with the steps of their columns (in band + 3 the M moves, see
// layoutOptions(); "wide" names the step that does) and the measures for a
// sketch true to scale:
// "widgets": [{"id": "mvg", "min": "M", "band": true}, ...] (the weather
// also has "band_alone": true, see WidgetType),
// "layouts": [{"steps": ["M", "S", "S"], "wide": "M"}, {"steps": ["L", "L"]}],
// "steps": {"S": 214, "M": 280, "L": 367},
// "screen": {"width": 800, "height": 480, "band": 205, "margin": 20, "gap": 26}
std::string settingsLayoutJson();

#if defined(ARDUINO)
// The setup help of the widgets on the settings page (requests to the MVG
// API and the Open-Meteo place search, see widgets/mvg/setup.h and
// widgets/weather/setup.h).
std::vector<core::SettingsRoute> settingsRoutes();
#endif

// The widget with this ID, nullptr if there is none.
const WidgetType* findWidgetType(std::string_view id);

}  // namespace widgets
