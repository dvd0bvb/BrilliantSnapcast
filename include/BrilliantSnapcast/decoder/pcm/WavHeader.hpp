#pragma once

#include <cstdint>

namespace brilliant::snapcast::decoder::pcm {
  struct WavHeader {
    std::uint32_t identifier;
    std::uint32_t fileSize;
    std::uint32_t format;  // Always WAVE
    std::uint32_t formatBlockId;
    std::uint32_t blockSize;
    std::uint16_t audioFormat;
    std::uint16_t numChannels;
    std::uint32_t sampleRate;
    std::uint32_t bytesPerSec;    // sampleRate * bytesPerBlock
    std::uint16_t bytesPerBlock;  // numChannels * bitsPerSample / 8
    std::uint16_t bitsPerSample;
    std::uint32_t dataBlockId;
    std::uint32_t dataSize;
  };
}  // namespace brilliant::snapcast::decoder::pcm