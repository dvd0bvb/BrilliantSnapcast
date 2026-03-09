#pragma once

#include <cstdint>
#include <type_traits>

namespace brilliant::snapcast {

  namespace {
    template<class T>
    concept Arithmetic = std::is_arithmetic_v<T>;
  }

  /**
   * @brief Enumeration of bitdepth types. Value of each enumeration is the
   * corresponding number of bytes.
   *
   */
  enum class BitDepth : std::uint8_t {
    EIGHT = 1,
    SIXTEEN,
    TWENTYFOUR,
    THIRTYTWO
  };

  template<Arithmetic T>
  constexpr auto operator* (T t, BitDepth bitDepth) -> T {
    return t * static_cast<std::underlying_type_t<BitDepth>>(bitDepth);
  }

  template<Arithmetic T>
  constexpr auto operator* (BitDepth bitDepth, T t) -> T {
    return t * bitDepth;
  }

  template<Arithmetic T>
  constexpr auto operator/ (T t, BitDepth bitDepth) -> T {
    return t / static_cast<std::underlying_type_t<BitDepth>>(bitDepth);
  }

  template<Arithmetic T>
  constexpr auto operator/ (BitDepth bitDepth, T t) -> T {
    return static_cast<std::underlying_type_t<BitDepth>>(bitDepth) / t;
  }

}  // namespace brilliant::snapcast