#include "FakeClock.hpp"
#include <BrilliantSnapcast/AudioDataDevice.hpp>
#include <BrilliantSnapcast/TimeProvider.hpp>
#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// TODO(david): Beef up these unit tests. Should generate test values using excel or something

TEST(TestTimeProvider, testGetServerNow) {
  using namespace brilliant::snapcast;
  using namespace std::chrono_literals;

  BasicTimeProvider<FakeClock> timeProvider(0.01, 0.0, 1.001);

  FakeClock::instance().now_ = std::chrono::steady_clock::time_point(4000s);

  Base base{};
  base.sent = {.sec = 1000000, .usec = 0};
  base.received = {.sec = 1001000, .usec = 0};
  Time time{.sec = 5000, .usec = 0};
  timeProvider.addTime(base, time); // latency = (5000 - 1000) / 2 = 2000

  EXPECT_EQ(timeProvider.getServerNow(), 6000s);

  FakeClock::instance().now_ = std::chrono::steady_clock::time_point(4001s);

  base.sent = {.sec = 2000000, .usec = 0};
  base.received = {.sec = 2001200, .usec = 0};
  time.sec = 6000;
  timeProvider.addTime(base, time);  // latency = (6000 - 1200) / 2 = 2400

  EXPECT_EQ(timeProvider.getServerNow(), 6401s);

  FakeClock::instance().now_ = std::chrono::steady_clock::time_point(4002s);

  base.sent = {.sec = 3000000, .usec = 0};
  base.received = {.sec = 3001100, .usec = 0};
  time.sec = 7000;
  timeProvider.addTime(base, time);  // latency = (7000 - 1100) / 2 = 2950

  EXPECT_EQ(timeProvider.getServerNow(), 6952s);
}
