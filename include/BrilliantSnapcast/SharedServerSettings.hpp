#pragma once

#include <chrono>
#include <mutex>
#include <shared_mutex>

namespace brilliant::snapcast {

  class SharedServerSettings {
  public:
    auto getEndToEndLatency() const noexcept -> std::chrono::microseconds {
      std::shared_lock lock(_mutex);
      return _endToEndLatency;
    }

    void setEndToEndLatency(std::chrono::microseconds latency) noexcept {
      std::unique_lock lock(_mutex);
      _endToEndLatency = latency;
    }

    auto getVolume() const noexcept -> std::uint32_t {
      std::shared_lock lock(_mutex);
      return _volume;
    }

    void setVolume(std::uint32_t volume) noexcept {
      std::unique_lock lock(_mutex);
      _volume = volume;
    }

  private:
    mutable std::shared_mutex _mutex;
    std::chrono::microseconds _endToEndLatency{0};
    std::uint32_t _volume{100};
  };

}  // namespace brilliant::snapcast