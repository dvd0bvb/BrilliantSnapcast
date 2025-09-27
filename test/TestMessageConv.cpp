#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <random>
#include <ranges>

#include "BrilliantSnapcast/Message.hpp"
#include "BrilliantSnapcast/MessageConv.hpp"
#include "BrilliantSnapcast/MessageType.hpp"

TEST(TestMessageConv, testConvTime) {
  brilliant::snapcast::Time time{.sec = 0x12305678, .usec = 0xdeadbeef};

  std::array<std::byte, sizeof(brilliant::snapcast::Time)> buffer{};
  brilliant::snapcast::write(std::span(buffer), time);

  EXPECT_THAT(buffer, testing::ElementsAre(std::byte{0x78}, std::byte{0x56},
                                           std::byte{0x30}, std::byte{0x12},
                                           std::byte{0xef}, std::byte{0xbe},
                                           std::byte{0xad}, std::byte{0xde}));

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
          std::byte{0x03}, std::byte{0x00}, std::byte{0x04}, std::byte{0x00},
          std::byte{0x09}, std::byte{0x00}, std::byte{0x7b}, std::byte{0x00},
          std::byte{0x00}, std::byte{0x00}, std::byte{0xc8}, std::byte{0x01},
          std::byte{0x00}, std::byte{0x00}, std::byte{0xdb}, std::byte{0x03},
          std::byte{0x00}, std::byte{0x00}, std::byte{0x8e}, std::byte{0x02},
          std::byte{0x00}, std::byte{0x00}, std::byte{0x29}, std::byte{0x00},
          std::byte{0x00}, std::byte{0x00}));

  buffer = {std::byte{0x04}, std::byte{0x00}, std::byte{0x12}, std::byte{0x03},
            std::byte{0x54}, std::byte{0x06}, std::byte{0x67}, std::byte{0x45},
            std::byte{0x23}, std::byte{0x00}, std::byte{0xcd}, std::byte{0xab},
            std::byte{0x00}, std::byte{0x00}, std::byte{0x89}, std::byte{0x67},
            std::byte{0x45}, std::byte{0x00}, std::byte{0x21}, std::byte{0x43},
            std::byte{0x00}, std::byte{0x00}, std::byte{0x54}, std::byte{0x76},
            std::byte{0x00}, std::byte{0x00}};
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

  EXPECT_THAT(
      buffer,
      testing::ElementsAre(
          std::byte{0x0c}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
          std::byte{'a'}, std::byte{'b'}, std::byte{'c'}, std::byte{'d'},
          std::byte{'e'}, std::byte{'f'}, std::byte{'g'}, std::byte{'h'},
          std::byte{'i'}, std::byte{'j'}, std::byte{'k'}, std::byte{'l'}));

  buffer = {std::byte{0x0a}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
            std::byte{'f'},  std::byte{'e'},  std::byte{'d'},  std::byte{'c'},
            std::byte{'b'},  std::byte{'a'},  std::byte{'0'},  std::byte{'1'},
            std::byte{'2'},  std::byte{'3'}};

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
           std::vector{std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
                       std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
                       std::byte{0x00}, std::byte{0x00}, std::byte{0x22},
                       std::byte{0x00}, std::byte{0x00}, std::byte{0x00}},
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
  EXPECT_THAT(
      buffer,
      testing::ElementsAre(
          std::byte{0x04}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
          std::byte{'t'}, std::byte{'e'}, std::byte{'s'}, std::byte{'t'},
          std::byte{0x07}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
          std::byte{'t'}, std::byte{'e'}, std::byte{'s'}, std::byte{'t'},
          std::byte{'i'}, std::byte{'n'}, std::byte{'g'}));

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
