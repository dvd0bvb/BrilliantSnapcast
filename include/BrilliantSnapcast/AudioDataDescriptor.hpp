#pragma once

#include <chrono>
#include <cstdint>

namespace brilliant::snapcast {

  /**
   * @brief Descriptor for a chunk of audio data
   *
   */
  struct AudioDataDescriptor {
    /// Timestamp of the audio chunk
    std::chrono::microseconds timepoint;
    /// Size of the audio chunk in bytes
    std::uint32_t chunkSize;
  };

}  // namespace brilliant::snapcast