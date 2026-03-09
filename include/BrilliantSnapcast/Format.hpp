#pragma once

#include <cstdint>
#include <BrilliantSnapcast/BitDepth.hpp>

namespace brilliant::snapcast {

  using SampleRateType = std::uint32_t;
  using NumChannelType = std::uint8_t;

  struct Format {
    SampleRateType sampleRate;
    NumChannelType numChannels;
    BitDepth bitDepth;

    [[nodiscard]] auto getFrameSize() const -> std::uint32_t {
      return numChannels * bitDepth;
    }
  };

}  // namespace brilliant::snapcast