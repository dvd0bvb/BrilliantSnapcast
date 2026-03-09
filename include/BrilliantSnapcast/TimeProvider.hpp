#pragma once

#include <BrilliantSnapcast/Log.hpp>
#include <BrilliantSnapcast/Message.hpp>
#include <BrilliantSnapcast/StatsBuffer.hpp>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <shared_mutex>

namespace brilliant::snapcast {

  /**
   * @brief Tracks difference in time between the client and server and provides
   * the current estimated server time.
   *
   * Time messages sent by the server contain the server sent time and the
   * client received time in its header and the body contains the network
   * latency from the client to the server, as calculated by the server. To
   * calculate latency we use a Kalman filter to predict time offset and drift
   * between the server and client clocks. This method of time correction is
   * based on the Sendspin protocol implementation.
   *
   * @tparam Clock The clock type to use for getting current time
   */
  template <class Clock>
  class BasicTimeProvider {
  public:
    /// Deleted default constructor
    BasicTimeProvider() = delete;

    /**
     * @brief Construct a new Basic Time Provider object
     *
     * @param processStdDev Standard deviation of the offset process noise in
     * us, models clock jitter
     * @param processDriftStdDev Standard deviation of the drift process noise
     * in us, models frequency wander
     * @param forgetFactor Forgetting factor >1 applied to covariances when high
     * residuals are observed. Larger values correct faster but may be less
     * stable.
     * @param adaptiveCutoff Fraction of error that triggers adaptive
     * forgetting. Defaults to 0.75.
     * @param maxSamples Maximum number of samples to collect before forgetting
     * can occur. Defaults to 60.
     */
    BasicTimeProvider(double processStdDev, double processDriftStdDev,
                      double forgetFactor, double adaptiveCutoff = 0.75,
                      std::uint32_t maxSamples = 60)
        : _lastUpdate{},
          _count{},
          _maxSamples(maxSamples),
          _offset{},
          _drift{},
          _offsetCovariance{},
          _driftCovariance{},
          _offsetDriftCovariance{},
          _driftProcessVariance(processDriftStdDev * processDriftStdDev),
          _processVariance(processStdDev * processStdDev),
          _forgetVarianceFactor(forgetFactor),
          _adaptiveForgettingCutoff(adaptiveCutoff) {}

    /**
     * @brief Calculate and store latency based on the given Time message
     *
     * @param base The Time message header
     * @param time The Time message body
     */
    void addTime(const Base& base, const Time& time) {
      const auto received =
          static_cast<std::chrono::microseconds>(base.received);
      const auto sent = static_cast<std::chrono::microseconds>(base.sent);
      const auto clientToServer = static_cast<std::chrono::microseconds>(time);

      const auto measurement = (clientToServer + sent - received) / 2;
      const auto error = (clientToServer - sent + received) / 2;

      addTime(measurement, error);
      BS_LOG_DEBUG("drift {} offset {}", _drift, _offset);
    }

    void addTime(std::chrono::microseconds measure,
                 std::chrono::microseconds maxError) {
      std::unique_lock lock(_mutex);
      const auto measurement = static_cast<double>(measure.count());
      const auto error = static_cast<double>(maxError.count());

      const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
          Clock::now().time_since_epoch());
      const auto dt = static_cast<double>((now - _lastUpdate).count());
      _lastUpdate = now;

      const auto updateStdDev = error * error;
      const auto variance = updateStdDev * updateStdDev;

      if (_count == 0) {
        ++_count;
        _offset = measurement;
        _offsetCovariance = static_cast<double>(variance);
        _drift = 0.0;
      } else if (_count == 1) {
        ++_count;
        _drift = (measurement - _offset) / dt;
        _offset = measurement;
        _driftCovariance = (_offsetCovariance - variance) / (dt * dt);
        _offsetCovariance = variance;
      } else {
        if (_count < _maxSamples) {
          ++_count;
        }
        // state prediction
        // offset_k = F * [offset_k-1, drift_k-1]
        // F = [1, dt]
        //     [0, 1 ]
        const auto offset = _offset + _drift * dt;

        // innovation/residual
        const auto residual = measurement - offset;
        const auto forgetFactor =
            (_count >= _maxSamples &&
             (error * _adaptiveForgettingCutoff) > residual)
                ? _forgetVarianceFactor
                : 1.0;

        // Kalman filter: P_k = F * P_k-1 * transpose(F) + Q
        // P = [offset cov, offset drift cov]
        //     [offset drift cov, drift cov ]
        // Q = [offset process var, drift process var]
        // below is the expansion of the matrix ops
        const auto driftProcessVariance = dt * _driftProcessVariance;
        const auto driftCov =
            (_driftCovariance + driftProcessVariance) * forgetFactor;

        const auto offsetDriftVariance =
            (_offsetDriftCovariance + _driftCovariance * dt) * forgetFactor;

        const auto offsetProcessVariance = dt * _processVariance;
        const auto offsetCov =
            (_offsetCovariance + 2 * _offsetDriftCovariance * dt +
             _driftCovariance * dt * dt + offsetProcessVariance) *
            forgetFactor;

        // innovation/residual covariance
        const auto residualCov = offsetCov + driftCov;

        // kalman gain
        // gain = [ offsetCov, driftCov ] * 1/residualCov
        const auto offsetGain = offsetCov / residualCov;
        const auto driftGain = driftCov / residualCov;

        _offset = offset + offsetGain * residual;
        _drift += driftGain * residual;
        _driftCovariance = driftCov - driftGain * offsetDriftVariance;
        _offsetDriftCovariance = offsetDriftVariance - driftGain * offsetCov;
        _offsetCovariance = offsetCov - offsetGain * offsetCov;
      }
    }

    /**
     * @brief Get the estimated current server time
     *
     * @return The current server time as microseconds from its epoch
     */
    [[nodiscard]] auto getServerNow() const -> std::chrono::microseconds {
      const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
          Clock::now().time_since_epoch());
      return getServerTime(now);
    }

    [[nodiscard]] auto getServerTime(std::chrono::microseconds time) const
        -> std::chrono::microseconds {
      std::shared_lock lock(_mutex);
      return time + std::chrono::microseconds{
                        static_cast<std::chrono::microseconds::rep>(
                            std::round(_offset))};
    }

    [[nodiscard]] auto getOffset() const -> std::chrono::microseconds {
      std::shared_lock lock(_mutex);
      return std::chrono::microseconds{
          static_cast<std::chrono::microseconds::rep>(_offset)};
    }

    void reset() {
      std::unique_lock lock(_mutex);
      _count = 0;
      _offset = 0.0;
      _drift = 0.0;
      _offsetDriftCovariance = 0.0;
      _offsetCovariance = 0.0;
      _driftCovariance = 0.0;
    }

  private:
    /// The time of the last update
    std::chrono::microseconds _lastUpdate;
    /// Number of samples taken
    std::uint32_t _count;
    /// Maximum number of samples before forgetting is applied
    std::uint32_t _maxSamples;
    /// The time offset from the server
    double _offset;
    /// The time drift
    double _drift;
    /// Offset covariance
    double _offsetCovariance;
    /// Drift covariance
    double _driftCovariance;
    /// Offset-drift covariance
    double _offsetDriftCovariance;
    /// Drift process variance
    double _driftProcessVariance;
    /// Offset process variance
    double _processVariance;
    /// Forget variance factor
    double _forgetVarianceFactor;
    /// Fraction of error that triggers forgetting
    double _adaptiveForgettingCutoff;

    // TODO(david): I would prefer to make the mutex optional for systems that
    // don't/can't use it
    mutable std::shared_mutex _mutex;
  };

  /// Convenience type alias for a monotonic TimeProvider
  using TimeProvider = BasicTimeProvider<std::chrono::steady_clock>;

}  // namespace brilliant::snapcast
