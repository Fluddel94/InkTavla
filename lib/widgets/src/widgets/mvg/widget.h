#pragma once

// The MVG departures widget: a data source that fetches the departures of
// one station once a minute and a view that shows them as one list or in two
// named sections.

#include <ArduinoJson.h>

#include <string>
#include <vector>

#include "widgets/mvg/config.h"
#include "widgets/mvg/view.h"
#include "widgets/widget.h"

namespace widgets::mvg {

// Where the departures response comes from: the MVG API on the device, saved
// responses in tests.
class Source {
 public:
  virtual ~Source() = default;
  // Reads the departures response for the configured station into doc
  // (filtered, see readDepartures()). False if the request failed.
  virtual bool fetchDepartures(const Config& config, JsonDocument& doc) = 0;
  // Name of the station for the title (when the configuration has none).
  virtual bool fetchStationName(const std::string& station, std::string& name) {
    (void)station;
    (void)name;
    return false;
  }
};

// Data source: the departures of the configured station, split into the
// configured sections.
class MvgSource : public DataSource {
 public:
  // At most this many departures are kept per section (list: 10 rows).
  static constexpr size_t kMaxDepartures = 10;

  // api must outlive the source.
  MvgSource(Config config, Source& api);

  const char* id() const override { return "mvg"; }
  SetupMessage setupMessage(ui::Language language) const override;

  bool fetches() const override { return true; }
  // At second 1 of every minute (like the MVG departure display), retries
  // every 10 s.
  core::Schedule schedule() const override { return {60, 1, 10}; }
  bool isSetUp() const override { return !config_.station.empty(); }
  bool fetch(int64_t wall_s) override;

  const Config& config() const { return config_; }
  const std::vector<Section>& sections() const { return sections_; }
  // Name of the station: from the configuration, else from the API; "" if
  // neither is known (yet).
  const std::string& stationName() const;

 private:
  Config config_;
  Source& api_;
  std::vector<Section> sections_;
  std::string station_name_;  // from the API if the configuration has no name
};

// View in a column slot (style A): tile title with the station name and the
// departures as one list or in two named sections.
class DeparturesView : public View {
 public:
  // source must outlive the view.
  explicit DeparturesView(const MvgSource& source) : source_(source) {}

  std::vector<const DataSource*> sources() const override { return {&source_}; }
  std::string title(ui::Language language) const override;
  void draw(ui::Canvas& canvas, const DrawContext& context) override;

 private:
  const MvgSource& source_;
};

}  // namespace widgets::mvg
