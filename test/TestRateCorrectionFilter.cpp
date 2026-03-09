#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <BrilliantSnapcast/DurationConversion.hpp>
#include <BrilliantSnapcast/RateCorrectionFilter.hpp>
#include <BrilliantSnapcast/SharedServerSettings.hpp>
#include <BrilliantSnapcast/TimeProvider.hpp>
#include <algorithm>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>

#include "ByteLiteral.hpp"
#include "FakeClock.hpp"
#include "CircularBufferAdapter.hpp"

using namespace byte_literal;

void initLog() {
  boost::log::register_simple_formatter_factory<boost::log::trivial::severity_level, char>("Severity");
  boost::log::add_console_log(std::cout,
    boost::log::keywords::auto_flush = true, 
    boost::log::keywords::filter = boost::log::trivial::severity >= boost::log::trivial::severity_level::trace
  );
  boost::log::add_common_attributes();
}

TEST(TestRateCorrectionFilter, testInitialData) {
  using namespace brilliant::snapcast;
  using namespace std::chrono_literals;

  BasicTimeProvider<FakeClock> timeProvider{0.01, 0.0, 1.001};
  SharedServerSettings serverSettings;
  serverSettings.setEndToEndLatency(50ms);

  BasicRateCorrectionFilter<FakeClock> filter(timeProvider, serverSettings);

  CircularBufferAdapter<AudioDataDescriptor> descriptors(10);
  CircularBufferAdapter<std::byte> bytes(1000);
  AudioDataDevice device(descriptors, bytes);
  device.setFormat(
      {.sampleRate = 48000, .numChannels = 1, .bitDepth = BitDepth::EIGHT});

  std::vector<std::byte> buffer(100);
  EXPECT_EQ(filter.read(device, 0us, std::span(buffer)),
            static_cast<std::uint32_t>(buffer.size()));
  EXPECT_TRUE(std::ranges::all_of(
      buffer, [](std::byte b) { return b == std::byte{0}; }));
}

TEST(TestRateCorrectionFilter, testReadAfterWrite) {
  using namespace brilliant::snapcast;
  using namespace std::chrono_literals;

  BasicTimeProvider<FakeClock> timeProvider{0.01, 0.0, 1.001};
  SharedServerSettings serverSettings;
  serverSettings.setEndToEndLatency(0ms);

  BasicRateCorrectionFilter<FakeClock> filter(timeProvider, serverSettings);

  CircularBufferAdapter<AudioDataDescriptor> descriptors(10);
  CircularBufferAdapter<std::byte> bytes(1024);
  AudioDataDevice device(descriptors, bytes);
  device.setFormat(
      {.sampleRate = 48000, .numChannels = 1, .bitDepth = BitDepth::EIGHT});

  FakeClock::instance().now_ =
      std::chrono::steady_clock::time_point{std::chrono::microseconds{5000}};
  std::vector<std::byte> testData(192, 1_b);
  EXPECT_EQ(device.write(5000us, std::span<const std::byte>(testData)),
            static_cast<std::uint32_t>(testData.size()));

  std::vector<std::byte> out(192);
  EXPECT_EQ(filter.read(device, 0us, std::span(out)),
            static_cast<std::uint32_t>(out.size()));
  EXPECT_EQ(out, testData);
}

TEST(TestRateCorrectionFilter, testFrameCorrection) {
  using namespace brilliant::snapcast;
  using namespace std::chrono_literals;

  initLog();

  BasicTimeProvider<FakeClock> timeProvider{0.01, 0.0, 1.001};
  SharedServerSettings serverSettings;
  serverSettings.setEndToEndLatency(0ms);

  BasicRateCorrectionFilter<FakeClock> filter(timeProvider, serverSettings);

  CircularBufferAdapter<AudioDataDescriptor> descriptors(10);
  CircularBufferAdapter<std::byte> bytes(2048);
  AudioDataDevice device(descriptors, bytes);
  Format format{
      .sampleRate = 48000, .numChannels = 1, .bitDepth = BitDepth::EIGHT};
  device.setFormat(format);

  FakeClock::instance().now_ = std::chrono::steady_clock::time_point{500us};

  // scheduled for 500us in the future
  // 500us is 24 bytes in 48000:1:8 format
  std::vector<std::byte> writeBuffer(192, 1_b);
  EXPECT_EQ(device.write(1000us, std::span<const std::byte>(writeBuffer)),
            static_cast<std::uint32_t>(writeBuffer.size()));

  std::vector<std::byte> readBuffer(192);
  EXPECT_EQ(filter.read(device, 0us, std::span(readBuffer)),
            static_cast<std::uint32_t>(readBuffer.size()));

  EXPECT_TRUE(std::ranges::all_of(std::span(readBuffer).first(24),
                                  [](std::byte b) { return b == 0_b; }));
  EXPECT_THAT(
      std::span(readBuffer).last(192 - 24),
      testing::ElementsAreArray(std::span(writeBuffer).first(192 - 24)));

  // there is still a chunk from the first write with a start time of 4500us
  // (start time(1000us) + duration of chunk (4000us) - 500us)
  FakeClock::instance().now_ = std::chrono::steady_clock::time_point{6500us};

  writeBuffer.assign(1250, 2_b);
  EXPECT_EQ(device.write(5000us, std::span<const std::byte>(writeBuffer)),
            static_cast<std::uint32_t>(writeBuffer.size()));

  // 2ms offset gives 96 frames removed over 2.5 seconds, or 1 correction every 1250 frames
  // since the buffer needs to catch up, we expect it to drop 1 frame during this read
  readBuffer.resize(1274);
  EXPECT_EQ(filter.read(device, 0ms, std::span(readBuffer)), 1274u);
  EXPECT_TRUE(std::ranges::all_of(std::span(readBuffer).first(24), [](std::byte b) {
    return b == 1_b;
  }));
  EXPECT_TRUE(std::ranges::all_of(std::span(readBuffer).subspan(24, 1249), [](std::byte b) {
    return b == 2_b;
  }));
  EXPECT_EQ(readBuffer.back(), 0_b);
}
