#pragma once

// The MVG departures widget: fetches the departures of one station once a
// minute and shows them as one list or in two named sections.

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
};

class MvgWidget : public Widget {
 public:
  // At most this many departures are kept per section (list: 10 rows).
  static constexpr size_t kMaxDepartures = 10;

  MvgWidget(Config config, Source& source);

  const char* id() const override { return "mvg"; }
  std::string title(ui::Language language) const override;
  SetupMessage setupMessage(ui::Language language) const override;

  bool fetches() const override { return true; }
  // At second 1 of every minute (like the MVG departure display), retries
  // every 10 s.
  core::Schedule schedule() const override { return {60, 1, 10}; }
  bool isSetUp() const override { return !config_.station.empty(); }
  bool fetch(int64_t wall_s) override;

  void draw(ui::Canvas& canvas, const DrawContext& context) override;

 private:
  Config config_;
  Source& source_;
  std::vector<Section> sections_;
};

}  // namespace widgets::mvg
