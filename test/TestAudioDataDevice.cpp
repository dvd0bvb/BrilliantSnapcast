#include <BrilliantSnapcast/AudioDataDevice.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "ByteLiteral.hpp"
#include "CircularBufferAdapter.hpp"

using namespace byte_literal;

TEST(TestAudioDataDevice, testReadWrite) {
  using namespace brilliant::snapcast;
  using namespace std::chrono_literals;

  CircularBufferAdapter<AudioDataDescriptor> descriptors(10);
  CircularBufferAdapter<std::byte> bytes(1024);
  AudioDataDevice device(descriptors, bytes);
  device.setFormat({.sampleRate=48000, .numChannels=1, .bitDepth=BitDepth::EIGHT});

  std::vector<std::byte> data{0x01_b, 0x02_b, 0x03_b, 0x04_b};

  EXPECT_EQ(device.write(1000us, std::span<const std::byte>(data)), 4u);

  data.clear();
  data.resize(4);

  EXPECT_EQ(device.read(1000us, std::span(data)), 4u);
  EXPECT_EQ(data[0], 0x01_b);
  EXPECT_EQ(data[1], 0x02_b);
  EXPECT_EQ(data[2], 0x03_b);
  EXPECT_EQ(data[3], 0x04_b);

  // Attempt to read again should yield 0 bytes as the device is empty
  EXPECT_EQ(device.read(1000us, std::span(data)), 0u);
}

TEST(TestAudioDataDevice, testMultipleInSingleOut) {
  using namespace brilliant::snapcast;
  using namespace std::chrono_literals;

  CircularBufferAdapter<AudioDataDescriptor> descriptors(10);
  CircularBufferAdapter<std::byte> bytes(2048);
  AudioDataDevice device(descriptors, bytes);
  device.setFormat({.sampleRate=48000, .numChannels=1, .bitDepth=BitDepth::EIGHT});

  for (int i = 0; i < 3; ++i) {
    std::vector<std::byte> data{static_cast<std::byte>(i + 1),
                                static_cast<std::byte>(i + 2),
                                static_cast<std::byte>(i + 3),
                                static_cast<std::byte>(i + 4)};
    EXPECT_EQ(device.write(std::chrono::microseconds{1000 + i * 100},
                           std::span<const std::byte>(data)), 4u);
  }

  std::vector<std::byte> combinedData(12);
  auto tp = device.peek();
  ASSERT_TRUE(tp.has_value());
  EXPECT_EQ(tp->timepoint, std::chrono::microseconds{1000});

  auto bytesRead = device.read(0us, std::span(combinedData));
  EXPECT_EQ(bytesRead, 12u);
  for (unsigned i = 0; i < 3; ++i) {
    EXPECT_EQ(combinedData[i * 4 + 0], static_cast<std::byte>(i + 1));
    EXPECT_EQ(combinedData[i * 4 + 1], static_cast<std::byte>(i + 2));
    EXPECT_EQ(combinedData[i * 4 + 2], static_cast<std::byte>(i + 3));
    EXPECT_EQ(combinedData[i * 4 + 3], static_cast<std::byte>(i + 4));
  }
}

TEST(TestAudioDataDevice, testUnderrun) {
  using namespace brilliant::snapcast;
  using namespace std::chrono_literals;

  CircularBufferAdapter<AudioDataDescriptor> descriptors(10);
  CircularBufferAdapter<std::byte> bytes(1024);
  AudioDataDevice device(descriptors, bytes);
  device.setFormat({.sampleRate=48000, .numChannels=1, .bitDepth=BitDepth::EIGHT});

  std::vector<std::byte> data{0x01_b, 0x02_b};
  EXPECT_EQ(device.write(1000us, std::span<const std::byte>(data)), 2u);

  data.clear();
  data.resize(4);
  auto bytesRead = device.read(0us, std::span(data));

  EXPECT_EQ(bytesRead, 2u);
  EXPECT_EQ(data[0], 0x01_b);
  EXPECT_EQ(data[1], 0x02_b);
}

TEST(TestAudioDataDevice, testSeek) {
  using namespace brilliant::snapcast;
  using namespace std::chrono_literals;

  CircularBufferAdapter<AudioDataDescriptor> descriptors(10);
  CircularBufferAdapter<std::byte> bytes(1024);
  AudioDataDevice device(descriptors, bytes);
  device.setFormat({.sampleRate=48000, .numChannels=1, .bitDepth=BitDepth::EIGHT});

  for (int i = 0; i < 5; ++i) {
    std::vector<std::byte> data{static_cast<std::byte>(i * 4),
                                static_cast<std::byte>(i * 4 + 1),
                                static_cast<std::byte>(i * 4 + 2),
                                static_cast<std::byte>(i * 4 + 3)};
    EXPECT_EQ(device.write(std::chrono::microseconds{1000 + i * 100},
                           std::span<const std::byte>(data)), 4u);
  }

  // Skip first two chunks
  device.seekToNextChunk();
  device.seekToNextChunk();

  std::vector<std::byte> data(4);
  EXPECT_EQ(device.read(0us, std::span(data)), 4u);
  // after skipping two chunks, we should read the third chunk (timepoint 1200us)
  EXPECT_EQ(data[0], 0x08_b);
  EXPECT_EQ(data[1], 0x09_b);
  EXPECT_EQ(data[2], 0x0a_b);
  EXPECT_EQ(data[3], 0x0b_b);

  // peek should return the next chunk's timepoint
  auto tp = device.peek();
  ASSERT_TRUE(tp.has_value());
  EXPECT_EQ(tp->timepoint, std::chrono::microseconds{1300});
}
