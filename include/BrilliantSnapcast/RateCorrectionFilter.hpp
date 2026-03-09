#pragma once

#include <BrilliantSnapcast/BitDepth.hpp>
#include <BrilliantSnapcast/DurationConversion.hpp>
#include <BrilliantSnapcast/Log.hpp>
#include <BrilliantSnapcast/PcmManipulation.hpp>
#include <BrilliantSnapcast/StatsBuffer.hpp>
#include <BrilliantSnapcast/TimeProvider.hpp>
#include <BrilliantSnapcast/SharedServerSettings.hpp>
#include <chrono>

namespace brilliant::snapcast {

  namespace {
    constexpr std::chrono::duration<double, std::chrono::seconds::period> CORRECTION_DURATION{2.5};

    constexpr double MAX_CORRECTION_FACTOR = 0.05;

    constexpr std::chrono::milliseconds REANCHOR_THRESHOLD{500};

    constexpr std::chrono::seconds REANCHOR_COOLDOWN{5};

    constexpr std::chrono::microseconds CORRECTION_THRESHOLD{50};
  }  // namespace

  /**
   * @brief Filter that performs rate correction on audio data streams
   *
   * @tparam Clock The clock type used by the TimeProvider
   */
  template <class Clock>
  class BasicRateCorrectionFilter {
  public:
    /**
     * @brief Default constructor deleted
     */
    BasicRateCorrectionFilter() = delete;

    /**
     * @brief Construct a new Rate Correction Filter object
     *
     * @param timeProvider Reference to the time provider
     * @param chunkSize Size of audio data chunks in bytes
     * @param mr Pointer to the memory resource to use for allocations
     */
    BasicRateCorrectionFilter(BasicTimeProvider<Clock>& timeProvider, SharedServerSettings& serverSettings)
        : _timeProvider(&timeProvider),
          _serverSettings(&serverSettings),
          _syncFilter(0.01, 0.0, 1.001),
          _correctionCooldown(0),
          _correctionInterval(0),
          _correctionInNFrames(0),
          _dropFrames(false),
          _lastReanchorTime{} {}

    /**
     * @brief Read audio data chunks with rate correction
     *
     * @tparam Source Type of the source to read from
     * @param src Reference to the source
     * @param dacLatency The DAC delay
     * @param buffer The buffer to read into
     * @return Number of bytes actually read
     */
    template <class Source, std::size_t Extent>
    auto read(Source& src, std::chrono::microseconds dacLatency,
              std::span<std::byte, Extent> buffer) -> std::uint32_t {
      using namespace std::chrono_literals;

      if (auto descriptor = src.peek(); descriptor) {
        const auto endToEndLatency = _serverSettings->getEndToEndLatency();
        auto scheduledTime = descriptor->timepoint + endToEndLatency - dacLatency;
        const auto expectedTime = _timeProvider->getServerNow() - dacLatency;

        if (_lastReanchorTime == 0us) {
          while (scheduledTime < expectedTime) {
            // scheduled time is in the past
            src.seekToNextChunk();
            descriptor = src.peek();
            if (descriptor) {
              scheduledTime = descriptor->timepoint + endToEndLatency - dacLatency;
            } else {
              // queue is empty
              BS_LOG_INFO("Emptied out the queue dumping old chunks");
              _correctionCooldown = 0;
              std::ranges::fill(buffer, std::byte{});
              return static_cast<std::uint32_t>(buffer.size());
            }
          }

          const auto numZeroBytes = static_cast<std::size_t>(durationToNumBytes(
              scheduledTime - expectedTime, src.getFormat()));
          if (numZeroBytes > buffer.size()) {
            _correctionCooldown = 0;
            BS_LOG_DEBUG("Filled with 0s waiting for chunk to mature in {}", scheduledTime - expectedTime);
            std::ranges::fill(buffer, std::byte{});
          } else {
            BS_LOG_DEBUG("Partial fill with 0s");
            _correctionCooldown = 0;
            std::ranges::fill_n(buffer.begin(),
                                static_cast<std::int32_t>(numZeroBytes),
                                std::byte{});
            src.read(dacLatency, buffer.subspan(numZeroBytes));
            _lastReanchorTime = expectedTime;
          }
          return static_cast<std::uint32_t>(buffer.size());
        } else {
          _syncFilter.addTime(scheduledTime - expectedTime, 5ms);

          if (_correctionCooldown <= 0) {
            const auto error = _syncFilter.getOffset();
            const auto absError = std::chrono::abs(error);
            if (absError > REANCHOR_THRESHOLD &&
                (expectedTime - _lastReanchorTime) > REANCHOR_COOLDOWN) {
              // hard sync on next read
              BS_LOG_INFO("Hard sync on next read. error {}", absError);
              _lastReanchorTime = 0us;
              _syncFilter.reset();
              std::ranges::fill(buffer, std::byte{});
              return static_cast<std::uint32_t>(buffer.size());
            } else if (absError > CORRECTION_THRESHOLD) {
              const auto framesError =
                  durationToFrames(absError, src.getFormat().sampleRate);
              const auto correctionsPerSecond =
                  std::min(static_cast<double>(framesError) /
                               CORRECTION_DURATION.count(),
                           src.getFormat().sampleRate * MAX_CORRECTION_FACTOR);
              // correction interval must be at least 2 or dropping logic will
              // run forever
              _correctionInterval =
                  static_cast<std::uint32_t>(std::round(std::max(
                      src.getFormat().sampleRate / correctionsPerSecond, 2.0)));
              _correctionInNFrames = _correctionInterval;
              // error < 0 means we're behind, drop frames to catch up
              // otherwise we're ahead so insert frames to slow down and allow
              // DAC to catch up
              _dropFrames = error < 0us;
              _correctionCooldown = static_cast<std::uint32_t>(
                  std::round(correctionsPerSecond *
                             static_cast<double>(CORRECTION_DURATION.count())));
              BS_LOG_DEBUG("Frames error {} correction interval {} correction in N frames {}", framesError, _correctionInterval, _correctionInNFrames);
            }
          }

          if (_correctionCooldown == 0) {
            // no correction needed
            BS_LOG_TRACE("Reading data with no modification");
            src.read(dacLatency, buffer);
          } else {
            if (const auto framesToRead = buffer.size() / src.getFormat().getFrameSize(); 
              framesToRead <= _correctionInNFrames) {
              
              src.read(dacLatency, buffer);
              _correctionInNFrames -= framesToRead;
            } else {
              const auto frameSize = src.getFormat().getFrameSize();
              std::uint32_t framesRead = 0;
              while (framesRead < framesToRead && _correctionCooldown > 0) {
                src.read(dacLatency, buffer.subspan(framesRead * frameSize, _correctionInNFrames * frameSize));
                framesRead += _correctionInNFrames;
                --_correctionCooldown;
                if (_dropFrames) {
                  // last frame will be copied over
                  --framesRead;
                } else {
                  // copy last frame
                  std::ranges::copy(buffer.subspan(framesRead * frameSize, frameSize), buffer.begin() + (framesRead + 1) * frameSize);
                  ++framesRead; 
                }
                _correctionInNFrames = _correctionInterval;
                if (const auto diff = (framesToRead - framesRead); diff <= _correctionInNFrames) {
                  src.read(dacLatency, buffer.subspan(framesRead * frameSize, diff * frameSize));
                  _correctionInNFrames -= diff;
                  framesRead += diff;
                }
              }
            }
          }
        }
        return static_cast<std::uint32_t>(buffer.size());
      } else {
        // stream is empty, send back 0s
        _correctionCooldown = 0; // no need for correction if stream is empty
        std::ranges::fill(buffer, std::byte{});
        return static_cast<std::uint32_t>(buffer.size());
      }
    }

  private:
    /// Pointer to the time provider
    BasicTimeProvider<Clock>* _timeProvider;

    SharedServerSettings* _serverSettings;

    BasicTimeProvider<Clock> _syncFilter;

    std::uint32_t _correctionCooldown;

    std::uint32_t _correctionInterval;

    std::uint32_t _correctionInNFrames;

    bool _dropFrames;

    std::chrono::microseconds _lastReanchorTime;
  };

  /// Convenience type alias for monotonic RateCorrectionFilter
  using RateCorrectionFilter =
      BasicRateCorrectionFilter<std::chrono::steady_clock>;
   
}  // namespace brilliant::snapcast