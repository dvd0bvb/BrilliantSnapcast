#pragma once

#include "BrilliantSnapcast/decoder/DecoderError.hpp"
#include <BrilliantSnapcast/BitDepth.hpp>
#include <BrilliantSnapcast/Format.hpp>
#include <BrilliantSnapcast/decoder/pcm/WavHeader.hpp>
#include <chrono>
#include <cstring>
#include <expected>
#include <span>
#include <system_error>

namespace brilliant::snapcast::decoder::pcm {

  class PcmDecoder {
  public:
    
    template <class Source, std::size_t Extent>
    auto decode(Source& source, std::chrono::microseconds dacLatency, std::span<std::byte, Extent> buffer) 
      -> std::tuple<std::uint32_t, std::error_code> {
      const auto read = source.read(dacLatency, buffer);
      return {read, std::error_code{}};
    }

    template <class Source, class Sink>
    auto decode(Source& source, Sink& sink) -> std::tuple<std::uint32_t, std::error_code> {
      using namespace std::chrono_literals;

      if (auto descriptor = source.peek()) {
        std::uint32_t written{};
        while (written < descriptor->chunkSize) {
          const auto toRead = std::min(static_cast<std::uint32_t>(_buffer.size()), descriptor->chunkSize - written);
          const auto actualRead = source.read(0us, std::span(_buffer.data(), toRead));
          if (actualRead == 0u) {
            // somehow read nothing, source may be empty
            break;
          }
          written += sink.append(std::span<const std::byte>(_buffer.data(), actualRead));
        }
        sink.commitAppend(descriptor->timepoint, written);
        return std::make_tuple(written, std::error_code{});
      }
      return std::make_tuple(0u, std::error_code{});
    }

    template <std::size_t Extent>
    auto getFormat(std::span<const std::byte, Extent> input) const
        -> std::expected<Format, std::error_code> {
      if (input.size() != sizeof(WavHeader)) {
        return std::unexpected(
            std::make_error_code(std::errc::invalid_argument));
      }

      WavHeader header{};
      std::memcpy(&header, input.data(), sizeof(WavHeader));
      if (header.bitsPerSample != 8 && header.bitsPerSample != 16 &&
          header.bitsPerSample != 24 && header.bitsPerSample != 32) {
        return std::unexpected(
            std::make_error_code(DecoderErrc::INVALID_BIT_DEPTH));
      }

      return Format{
          .sampleRate = header.sampleRate,
          .numChannels = static_cast<NumChannelType>(header.numChannels),
          .bitDepth = static_cast<BitDepth>(header.bitsPerSample / 8)};
    }

  private:
    std::array<std::byte, 512> _buffer;

  };

}  // namespace brilliant::snapcast::decoder::pcm
