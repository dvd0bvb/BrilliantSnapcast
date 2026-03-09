#pragma once

#include <BrilliantSnapcast/SharedServerSettings.hpp>
#include <chrono>

namespace brilliant::snapcast {

  class ServerSettingsFilter {
  public:
    ServerSettingsFilter(SharedServerSettings& serverSettings) :
      _serverSettings(&serverSettings) {}

    void setEndToEndLatency(std::chrono::microseconds latency) {
      _serverSettings->setEndToEndLatency(latency);
    }

    void setVolume(std::uint32_t volume) {
      _serverSettings->setVolume(volume);
    }

  private:
    SharedServerSettings* _serverSettings;
  };

  template <class Pipeline>
  void setEndToEndLatency(Pipeline& pipeline, std::chrono::milliseconds latency) {
    pipeline.visit([latency](ServerSettingsFilter& filter) {
      filter.setEndToEndLatency(latency);
    });
  }

  template <class Pipeline>
  void setVolume(Pipeline& pipeline, std::uint32_t volume) {
    pipeline.visit([volume](ServerSettingsFilter& filter) {
      filter.setVolume(volume);
    });
  }

}  // namespace brilliant::snapcast