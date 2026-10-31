#include "widgets/mvg/widget.h"

#include <algorithm>
#include <utility>

#include "core/psram.h"
#include "widgets/mvg/departures.h"
#include "widgets/mvg/sections.h"

namespace widgets::mvg {

MvgSource::MvgSource(Config config, Source& api) : config_(std::move(config)), api_(api) {}

SetupMessage MvgSource::setupMessage(ui::Language language) const {
  return {ui::text(language, ui::Text::kStationMissing),
          ui::text(language, ui::Text::kSetUpOnSettingsPage)};
}

const std::string& MvgSource::stationName() const {
  return config_.name.empty() ? station_name_ : config_.name;
}

bool MvgSource::fetch(int64_t wall_s) {
  (void)wall_s;
  // One request per station: the list or the sections of the widget's
  // station, then a section with a station of its own (two requests a minute
  // for two stations). If one fails, the old data stays.
  std::vector<Section> sections(config_.sections.empty() ? 1 : config_.sections.size());
  std::vector<std::string> done;
  const size_t count = config_.sections.empty() ? 1 : config_.sections.size();
  for (size_t i = 0; i < count; i++) {
    const std::string& station =
        config_.sections.empty() ? config_.station : config_.sectionStation(i);
    if (std::find(done.begin(), done.end(), station) != done.end()) continue;
    done.push_back(station);
    Config part = config_;
    part.station = station;
    std::vector<size_t> index;  // configured position of every section of part
    if (!config_.sections.empty()) {
      part.sections.clear();
      for (size_t j = 0; j < config_.sections.size(); j++) {
        if (config_.sectionStation(j) != station) continue;
        part.sections.push_back(config_.sections[j]);
        index.push_back(j);
      }
    }
    JsonDocument doc(core::jsonAllocator());  // large: PSRAM on the device
    if (!api_.fetchDepartures(part, doc)) return false;
    if (!doc.is<JsonArrayConst>()) return false;  // not a departures response
    std::vector<Section> result = departureSections(doc, part, kMaxDepartures);
    if (index.empty()) {
      sections = std::move(result);
    } else {
      for (size_t j = 0; j < index.size() && j < result.size(); j++) {
        sections[index[j]] = std::move(result[j]);
      }
    }
  }
  // Two stations: every section title gets the location pin.
  if (config_.hasOwnStations()) {
    for (Section& s : sections) s.own_station = true;
  }
  sections_ = std::move(sections);
  // The station name once, after the departures (a failure here is not a
  // failure of the update; it is tried again next time).
  if (config_.name.empty() && station_name_.empty() && !config_.station.empty()) {
    api_.fetchStationName(config_.station, station_name_);
  }
  return true;
}

}  // namespace widgets::mvg
