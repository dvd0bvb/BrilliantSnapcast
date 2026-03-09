#include "ByteLiteral.hpp"
#include "gmock/gmock.h"
#include <BrilliantSnapcast/Message.hpp>
#include <BrilliantSnapcast/MessageConv.hpp>
#include <BrilliantSnapcast/MessageType.hpp>
#include <algorithm>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>
#include <random>
#include <ranges>

using namespace byte_literal;

TEST(TestMessageConv, testConvTime) {
  brilliant::snapcast::Time time{.sec = 0x12305678, .usec = 0xdeadbeef};

  std::array<std::byte, sizeof(brilliant::snapcast::Time)> buffer{};
  brilliant::snapcast::write(std::span(buffer), time);

  EXPECT_THAT(buffer, testing::ElementsAre(0x78_b, 0x56_b, 0x30_b, 0x12_b,
                                           0xef_b, 0xbe_b, 0xad_b, 0xde_b));

  buffer = std::array<std::byte, sizeof(brilliant::snapcast::Time)>{
      static_cast<std::byte>(0x98), static_cast<std::byte>(0x43),
      static_cast<std::byte>(0xab), static_cast<std::byte>(0xcd),
      static_cast<std::byte>(0xaf), static_cast<std::byte>(0x6d),
      static_cast<std::byte>(0x4f), static_cast<std::byte>(0xdd)};
  brilliant::snapcast::read(std::span(buffer), time);

  EXPECT_EQ(time.sec, 0xcdab4398);
  EXPECT_EQ(time.usec, 0xdd4f6daf);
}

TEST(TestMessageConv, testConvBase) {
  std::array<std::byte, sizeof(brilliant::snapcast::Base)> buffer{};
  brilliant::snapcast::Base base{
      .type = brilliant::snapcast::MessageType::SERVER_SETTINGS,
      .id = 4,
      .refersTo = 9,
      .sent = brilliant::snapcast::Time{.sec = 123, .usec = 456},
      .received = brilliant::snapcast::Time{.sec = 987, .usec = 654},
      .size = 41};

  brilliant::snapcast::write(std::span(buffer), base);

  EXPECT_THAT(
      buffer,
      testing::ElementsAre(
          0x03_b, 0x00_b, 0x04_b, 0x00_b, 0x09_b, 0x00_b, 0x7b_b, 0x00_b,
          0x00_b, 0x00_b, 0xc8_b, 0x01_b, 0x00_b, 0x00_b, 0xdb_b, 0x03_b,
          0x00_b, 0x00_b, 0x8e_b, 0x02_b, 0x00_b, 0x00_b, 0x29_b, 0x00_b,
          0x00_b, 0x00_b, 0x00_b, 0x00_b));  // has 2 extra bytes for padding

  buffer = {0x04_b, 0x00_b, 0x12_b, 0x03_b, 0x54_b, 0x06_b, 0x67_b,
            0x45_b, 0x23_b, 0x00_b, 0xcd_b, 0xab_b, 0x00_b, 0x00_b,
            0x89_b, 0x67_b, 0x45_b, 0x00_b, 0x21_b, 0x43_b, 0x00_b,
            0x00_b, 0x54_b, 0x76_b, 0x00_b, 0x00_b};
  brilliant::snapcast::read(std::span(buffer), base);

  EXPECT_EQ(base.type, brilliant::snapcast::MessageType::TIME);
  EXPECT_EQ(base.id, 0x0312);
  EXPECT_EQ(base.refersTo, 0x0654);
  EXPECT_EQ(base.sent.sec, 0x0023'4567);
  EXPECT_EQ(base.sent.usec, 0x0000'abcd);
  EXPECT_EQ(base.received.sec, 0x0045'6789);
  EXPECT_EQ(base.received.usec, 0x0000'4321);
  EXPECT_EQ(base.size, 0x7654);
}

TEST(TestMessageConv, testConvHello) {
  brilliant::snapcast::Message message(
      brilliant::snapcast::Hello{"abcdefghijkl"});

  std::array<std::byte, 16> buffer{};
  brilliant::snapcast::write(std::span(buffer), message);

  EXPECT_THAT(buffer, testing::ElementsAre(0x0c_b, 0x00_b, 0x00_b, 0x00_b,
                                           0x61_b, 0x62_b, 0x63_b, 0x64_b,
                                           0x65_b, 0x66_b, 0x67_b, 0x68_b,
                                           0x69_b, 0x6a_b, 0x6b_b, 0x6c_b));

  buffer = {0x0a_b, 0x00_b, 0x00_b, 0x00_b, 0x66_b, 0x65_b, 0x64_b,
            0x63_b, 0x62_b, 0x61_b, 0x30_b, 0x31_b, 0x32_b, 0x33_b};

  message = brilliant::snapcast::read(std::span(buffer),
                                      brilliant::snapcast::MessageType::HELLO);

  auto& msg = std::get<brilliant::snapcast::Hello>(message);
  EXPECT_EQ(msg.size, 10);
  EXPECT_EQ(std::string_view(msg.payload, msg.size), "fedcba0123");
}

TEST(TestMessageConv, testConvWirechunk) {
  std::vector<std::byte> buffer{};

  std::default_random_engine gen(std::random_device{}());
  std::uniform_int_distribution<unsigned char> dist(
      std::numeric_limits<unsigned char>::min(),
      std::numeric_limits<unsigned char>::max());

  constexpr auto size = 34;
  std::vector<std::byte> data(size);
  std::ranges::generate(data, [&dist, &gen]() {
    const auto c = dist(gen);
    return static_cast<std::byte>(c);
  });

  brilliant::snapcast::Message message =
      brilliant::snapcast::WireChunk(std::span(data));
  buffer.resize(sizeof(brilliant::snapcast::Time) + sizeof(std::uint32_t) +
                size);
  brilliant::snapcast::write(std::span(buffer), message);

  constexpr auto offset =
      sizeof(brilliant::snapcast::Time) + sizeof(std::uint32_t);
  for (auto [expected, actual] : std::views::zip(
           std::vector{0x00_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b,
                       0x00_b, 0x22_b, 0x00_b, 0x00_b, 0x00_b},
           std::views::take(buffer, offset))) {
    EXPECT_EQ(expected, actual);
  }

  for (auto [expected, actual] :
       std::ranges::zip_view(data, std::views::drop(buffer, offset))) {
    EXPECT_EQ(expected, actual);
  }
}

TEST(TestMessageConv, testConvCodecHeader) {
  std::vector<std::byte> buffer;

  std::string t{"testing"};
  brilliant::snapcast::Message message = brilliant::snapcast::CodecHeader(
      "test", std::span(reinterpret_cast<std::byte*>(t.data()), 7));
  {
    auto& msg = std::get<brilliant::snapcast::CodecHeader>(message);
    ASSERT_EQ(msg.codecSize, 4);
    ASSERT_EQ(msg.size, 7);
    buffer.resize(sizeof(msg.codecSize) + msg.codecSize + sizeof(msg.size) +
                  msg.size);
  }
  brilliant::snapcast::write(std::span(buffer), message);
  EXPECT_THAT(buffer,
              testing::ElementsAre(0x04_b, 0x00_b, 0x00_b, 0x00_b, 0x74_b,
                                   0x65_b, 0x73_b, 0x74_b, 0x07_b, 0x00_b,
                                   0x00_b, 0x00_b, 0x74_b, 0x65_b, 0x73_b,
                                   0x74_b, 0x69_b, 0x6e_b, 0x67_b));

  message = brilliant::snapcast::CodecHeader{};
  message = brilliant::snapcast::read(
      std::span(buffer), brilliant::snapcast::MessageType::CODEC_HEADER);

  {
    auto& msg = std::get<brilliant::snapcast::CodecHeader>(message);
    EXPECT_EQ(msg.codecSize, 4);
    EXPECT_EQ(std::string_view(msg.codec, msg.codecSize), "test");
    EXPECT_EQ(msg.size, 7);
    EXPECT_EQ(
        std::string_view(reinterpret_cast<const char*>(msg.payload), msg.size),
        "testing");
  }
}

TEST(TestMessageConv, testConvServerSettings) {
  constexpr std::string_view payload{"This is some data"};
  brilliant::snapcast::Message message =
      brilliant::snapcast::ServerSettings(payload);

  std::array<std::byte, payload.size() + 4> buffer{};
  brilliant::snapcast::write(std::span(buffer), message);

  EXPECT_THAT(buffer,
              testing::ElementsAre(
                  0x11_b, 0x00_b, 0x00_b, 0x00_b, 0x54_b, 0x68_b, 0x69_b,
                  0x73_b, 0x20_b, 0x69_b, 0x73_b, 0x20_b, 0x73_b, 0x6f_b,
                  0x6d_b, 0x65_b, 0x20_b, 0x64_b, 0x61_b, 0x74_b, 0x61_b));

  std::uint32_t size = 10;
  constexpr std::string_view data{"ten bytes_"};
  std::memcpy(buffer.data(), &size, sizeof(size));
  std::memcpy(buffer.data() + sizeof(size), data.data(), data.size());

  message = brilliant::snapcast::read(
      std::span(buffer), brilliant::snapcast::MessageType::SERVER_SETTINGS);

  auto& msg = std::get<brilliant::snapcast::ServerSettings>(message);
  EXPECT_EQ(msg.size, size);
  EXPECT_EQ(std::string_view(msg.payload, msg.size), data);
}

TEST(TestMessageConv, testConvClientInfoSettings) {
  constexpr std::string_view payload{"This is a test"};
  brilliant::snapcast::Message message =
      brilliant::snapcast::ClientInfo(payload);

  std::array<std::byte, payload.size() + 4> buffer{};
  brilliant::snapcast::write(std::span(buffer), message);

  EXPECT_THAT(buffer, testing::ElementsAre(
                          0x0e_b, 0x00_b, 0x00_b, 0x00_b, 0x54_b, 0x68_b,
                          0x69_b, 0x73_b, 0x20_b, 0x69_b, 0x73_b, 0x20_b,
                          0x61_b, 0x20_b, 0x74_b, 0x65_b, 0x73_b, 0x74_b));

  std::uint32_t size = 9;
  constexpr std::string_view data{"ninebytes"};
  std::memcpy(buffer.data(), &size, sizeof(size));
  std::memcpy(buffer.data() + sizeof(size), data.data(), data.size());

  message = brilliant::snapcast::read(
      std::span(buffer), brilliant::snapcast::MessageType::CLIENT_INFO);

  auto& msg = std::get<brilliant::snapcast::ClientInfo>(message);
  EXPECT_EQ(msg.size, size);
  EXPECT_EQ(std::string_view(msg.payload, msg.size), data);
}

TEST(TestMessageConv, testConvError) {
  std::vector<std::byte> buffer(1024);

  brilliant::snapcast::Message message = brilliant::snapcast::Error(
      42, "ErrorString", "This is the error message");
  brilliant::snapcast::write(std::span(buffer), message);
  EXPECT_THAT(
      std::span(buffer).subspan(0, 48),
      testing::ElementsAre(
          0x2a_b, 0x00_b, 0x00_b, 0x00_b, 0x0b_b, 0x00_b, 0x00_b, 0x00_b,
          0x45_b, 0x72_b, 0x72_b, 0x6f_b, 0x72_b, 0x53_b, 0x74_b, 0x72_b,
          0x69_b, 0x6e_b, 0x67_b, 0x19_b, 0x00_b, 0x00_b, 0x00_b, 0x54_b,
          0x68_b, 0x69_b, 0x73_b, 0x20_b, 0x69_b, 0x73_b, 0x20_b, 0x74_b,
          0x68_b, 0x65_b, 0x20_b, 0x65_b, 0x72_b, 0x72_b, 0x6f_b, 0x72_b,
          0x20_b, 0x6d_b, 0x65_b, 0x73_b, 0x73_b, 0x61_b, 0x67_b, 0x65_b));

  message = brilliant::snapcast::read(std::span(buffer),
                                      brilliant::snapcast::MessageType::ERROR);

  {
    auto& msg = std::get<brilliant::snapcast::Error>(message);
    EXPECT_EQ(msg.errorCode, 42);
    EXPECT_EQ(msg.errorSize, 11);
    EXPECT_EQ(std::string_view(msg.error, msg.errorSize), "ErrorString");
    EXPECT_EQ(msg.errorMessageSize, 25);
    EXPECT_EQ(std::string_view(msg.errorMessage, msg.errorMessageSize),
              "This is the error message");
  }
}
