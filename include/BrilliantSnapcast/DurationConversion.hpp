#pragma once

#include <BrilliantSnapcast/BitDepth.hpp>
#include <BrilliantSnapcast/Format.hpp>
#include <chrono>
#include <cmath>

namespace brilliant::snapcast {

  /**
   * @brief Convert a duration to number of frames
   *
   * @tparam Rep The duration underlying type
   * @tparam Period The duration period
   * @param duration The duration
   * @param sampleRate The sample rate
   * @return The number of samples which fit in the given duration
   */
  template <class Rep, class Period>
  constexpr auto durationToFrames(
      const std::chrono::duration<Rep, Period>& duration,
      SampleRateType sampleRate) -> std::int32_t {
    if constexpr (Period::den != 0) {
      return static_cast<std::int32_t>(duration.count() * sampleRate *
                                       Period::num / Period::den);
    } else {
      return static_cast<int32_t>(duration.count() * sampleRate * Period::num);
    }
  }

  /**
   * @brief Convert a duration to number of bytes
   *
   * @tparam Rep The underlying type of the duration
   * @tparam Period The duration period
   * @param duration The duration
   * @param format The audio data format
   * @return The number of bytes which fit in the given duration
   */
  template <class Rep, class Period>
  constexpr auto durationToNumBytes(
      const std::chrono::duration<Rep, Period>& duration, const Format& format)
      -> Rep {
    return durationToFrames(duration, format.sampleRate) * format.numChannels *
           format.bitDepth;
  }

  /**
   * @brief Calculate the duration of a number of audio frames
   *
   * @tparam Duration The duration type
   * @param frames The number of frames
   * @param sampleRate The audio sample rate
   * @return The duration of the number of frames
   */
  template <class Duration>
  constexpr auto framesToDuration(std::uint32_t frames,
                                  SampleRateType sampleRate) noexcept
      -> Duration {
    using Period = typename Duration::period;
    using Rep = typename Duration::rep;
    const auto duration =
        static_cast<Rep>(frames * Period::den / sampleRate / Period::num);
    return Duration{duration};
  }

  /**
   * @brief Calculate the duration of a given number of bytes
   *
   * @tparam Duration The duration type
   * @param bytes The number of bytes
   * @param format The audio data format
   * @return The duration of the number of bytes
   */
  template <class Duration>
  constexpr auto bytesToDuration(std::uint32_t bytes, const Format& format)
      -> Duration {
    return framesToDuration<Duration>(
        bytes / format.numChannels / format.bitDepth, format.sampleRate);
  }

}  // namespace brilliant::snapcast