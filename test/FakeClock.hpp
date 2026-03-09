#pragma once

#include <chrono>

class FakeClock {
public:
  static auto instance() -> FakeClock& {
    static FakeClock clock;
    return clock;
  }

  static auto now() -> std::chrono::steady_clock::time_point {
    return instance().now_;
  }

  std::chrono::steady_clock::time_point now_{};
};
