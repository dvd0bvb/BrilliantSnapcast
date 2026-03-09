#include <BrilliantSnapcast/DurationConversion.hpp>
#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

TEST(TestDurationConversion, testDurationToFrames) {
  using namespace brilliant::snapcast;

  constexpr std::chrono::milliseconds duration{500};
  constexpr std::uint32_t sampleRate = 48000;

  EXPECT_EQ(durationToFrames(duration, sampleRate), 24000);
}

TEST(TestDurationConversion, testDurationToNumBytes) {
  using namespace brilliant::snapcast;

  constexpr std::chrono::milliseconds duration{1000};
  const brilliant::snapcast::Format format{.sampleRate=44100, .numChannels=2, .bitDepth=BitDepth::SIXTEEN};

  EXPECT_EQ(durationToNumBytes(duration, format), 176400);
}

TEST(TestDurationConversion, testFramesToDuration) {
  using namespace brilliant::snapcast;

  constexpr std::uint32_t frames = 48 * 4;
  constexpr std::uint32_t sampleRate = 48000;

  EXPECT_EQ(framesToDuration<std::chrono::milliseconds>(frames, sampleRate), std::chrono::milliseconds{4});
}

TEST(TestDurationConversion, testBytesToDuration) {
  using namespace brilliant::snapcast;

  constexpr std::uint32_t numBytes = 176400;
  const brilliant::snapcast::Format format2{44100, 2, BitDepth::SIXTEEN};

  EXPECT_EQ(bytesToDuration<std::chrono::milliseconds>(numBytes, format2), std::chrono::milliseconds{1000});
}