#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <BrilliantSnapcast/DecoderFilter.hpp>
#include <BrilliantSnapcast/AudioDataDevice.hpp>
#include "BrilliantSnapcast/decoder/AnyDecoder.hpp"
#include "FakeDecoder.hpp"
#include "ByteLiteral.hpp"
#include "CircularBufferAdapter.hpp"

using namespace byte_literal;

TEST(DecoderFilterTest, testPassThrough) {
  using namespace brilliant::snapcast;
  using namespace std::chrono_literals;

  DecoderFilter<FakeDecoder> filter;
  CircularBufferAdapter<AudioDataDescriptor> descriptors(10);
  CircularBufferAdapter<std::byte> bytes(1024);
  AudioDataDevice source(descriptors, bytes);
  source.setFormat({.sampleRate=48000, .numChannels=1, .bitDepth=BitDepth::EIGHT});

  std::array<std::byte, 4> inputData{1_b, 2_b, 3_b, 4_b};
  source.write(0us, std::span<const std::byte>(inputData));

  std::array<std::byte, 4> outputData{};
  EXPECT_EQ(filter.read(source, 0us, std::span<std::byte>(outputData)), 4);

  // read output and verify passthrough contents
  EXPECT_EQ(outputData[0], 1_b);
  EXPECT_EQ(outputData[1], 2_b);
  EXPECT_EQ(outputData[2], 3_b);
  EXPECT_EQ(outputData[3], 4_b);
}

TEST(DecoderFilterTest, testDecoding) {
  using namespace brilliant::snapcast;
  using namespace std::chrono_literals;

  FakeDecoder decoder;
  DecoderFilter<FakeDecoder> filter(decoder);
  CircularBufferAdapter<AudioDataDescriptor> descriptors(10);
  CircularBufferAdapter<std::byte> bytes(1024);
  AudioDataDevice source(descriptors, bytes);
  source.setFormat({.sampleRate=48000, .numChannels=1, .bitDepth=BitDepth::EIGHT});

  std::array<std::byte, 4> inputData{1_b, 2_b, 3_b, 4_b};
  source.write(0us, std::span<const std::byte>(inputData));

  std::array<std::byte, 8> outputData{};
  EXPECT_EQ(filter.read(source, 0us, std::span<std::byte>(outputData)), 8);

  EXPECT_EQ(outputData[0], 1_b);
  EXPECT_EQ(outputData[1], 1_b);
  EXPECT_EQ(outputData[2], 2_b);
  EXPECT_EQ(outputData[3], 2_b);
  EXPECT_EQ(outputData[4], 3_b);
  EXPECT_EQ(outputData[5], 3_b);
  EXPECT_EQ(outputData[6], 4_b);
  EXPECT_EQ(outputData[7], 4_b);
}