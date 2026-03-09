#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <system_error>
#include "BrilliantSnapcast/Format.hpp"

class FakeDecoder {
public:
  template <class Source, std::size_t Extent>
  auto decode(Source& source, std::chrono::microseconds dacLatency, std::span<std::byte, Extent> output)
      -> std::tuple<std::uint32_t, std::error_code> {
    std::vector<std::byte> buffer((output.size() + 1) / 2);
    const auto size = source.read(dacLatency, std::span(buffer));
    for (size_t i = 0; i < size; ++i) {
      output[i * 2] = buffer[i];
      output[i * 2 + 1] = buffer[i];
    }
    return std::make_tuple(static_cast<std::uint32_t>(size * 2), std::error_code{});
  }

  template <std::size_t Extent>
  auto getFormat(std::span<const std::byte, Extent> /*buffer*/) {
    return format;
  }

  brilliant::snapcast::Format format;
};
