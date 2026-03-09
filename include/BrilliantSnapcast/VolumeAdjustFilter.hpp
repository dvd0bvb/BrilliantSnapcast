#pragma once

#include <BrilliantSnapcast/BitDepth.hpp>
#include <BrilliantSnapcast/Format.hpp>
#include <BrilliantSnapcast/SharedServerSettings.hpp>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <ranges>
#include <type_traits>

namespace brilliant::snapcast {

  class VolumeAdjustFilter {
  public:
    VolumeAdjustFilter(SharedServerSettings& serverSettings) : _serverSettings(&serverSettings) {}

    template <class Source, std::size_t Extent>
    auto read(Source& src, std::chrono::microseconds dacLatency,
              std::span<std::byte, Extent> buffer) {
      const auto rawVolume = _serverSettings->getVolume();
      if (rawVolume == 0) {
        // still need to read data from the underlying source
        const auto numRead = src.read(dacLatency, buffer);
        std::ranges::fill(buffer, std::byte{});
        return numRead;
      } else if (rawVolume == 100) {
        // passthrough if at full volume
        return src.read(dacLatency, buffer);
      }

      const Format& format = src.getFormat();
      const double volume = rawVolume / 100.0;
      //  (x/100)^2 makes adjustment at higher volume more gradual for a more
      //  natural sound
      // TODO(david): can we make the volume manipulation function customizable?
      // just need to provide function to map [0,100] -> [0.0, 1.0]
      const double factor = volume * volume;
      const auto sampleSizeBytes =
          static_cast<std::underlying_type_t<BitDepth>>(format.bitDepth);
      // use fixed point math for performance
      // factor is in the range [0, 1] so we can use sampleSizeBytes * 8 - 1 as
      // fractional bits
      const auto fractionalBits = sampleSizeBytes * 8 - 1;
      const auto fixedPointFactor = static_cast<std::int32_t>(
          std::round(factor * (1 << fractionalBits)));

      const auto numRead = src.read(dacLatency, buffer);
      switch (format.bitDepth) {
      case BitDepth::EIGHT:
        for (auto& byte : buffer) {
          auto asI8 = reinterpret_cast<std::int8_t*>(&byte);
          *asI8 = static_cast<std::int8_t>(std::clamp(
              (static_cast<std::int32_t>(*asI8) * fixedPointFactor) >>
                  fractionalBits,
              static_cast<std::int32_t>(
                  std::numeric_limits<std::int8_t>::max()),
              static_cast<std::int32_t>(
                  std::numeric_limits<std::int8_t>::min())));
        }
        break;
      case BitDepth::SIXTEEN:
        for (auto&& chunk : buffer | std::views::chunk(sampleSizeBytes)) {
          auto asI16 = reinterpret_cast<std::int16_t*>(&*chunk.begin());
          *asI16 = static_cast<std::int16_t>(std::clamp(
              (static_cast<std::int32_t>(*asI16) * fixedPointFactor) >>
                  fractionalBits,
              static_cast<std::int32_t>(
                  std::numeric_limits<std::int16_t>::min()),
              static_cast<std::int32_t>(
                  std::numeric_limits<std::int16_t>::max())));
        }
        break;
      case BitDepth::TWENTYFOUR: {
        constexpr std::int32_t I24_MAX = (1 << 24) - 1;
        constexpr std::int32_t I24_MIN = -I24_MAX;

        for (auto&& chunk : buffer | std::views::chunk(sampleSizeBytes)) {
          auto ptr = reinterpret_cast<std::uint8_t*>(&*chunk.begin());
          auto data = ptr[0] | ptr[1] << 8 | ptr[2] << 16;
          data = std::clamp((data * fixedPointFactor) >> fractionalBits,
                            I24_MIN, I24_MAX);
          std::memcpy(ptr, &data, sampleSizeBytes);
        }
      } break;
      case BitDepth::THIRTYTWO:
        for (auto&& chunk : buffer | std::views::chunk(sampleSizeBytes)) {
          auto asI32 = reinterpret_cast<std::int32_t*>(&*chunk.begin());
          // TODO(david): Investigate potential overflow issues, possibly do
          // math in i64
          *asI32 = std::clamp((*asI32 * fixedPointFactor) >> fractionalBits,
                              std::numeric_limits<std::int32_t>::min(),
                              std::numeric_limits<std::int32_t>::max());
        }
        break;
      }
      return numRead;
    }

  private:
    SharedServerSettings* _serverSettings;
  };

}  // namespace brilliant::snapcast
