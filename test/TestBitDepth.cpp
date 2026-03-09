#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <BrilliantSnapcast/BitDepth.hpp>

using namespace brilliant::snapcast;

TEST(TestBitDepth, testOperators) {
  // EXPECT_EQ(BitDepth::EIGHT * 2, 2);
  // EXPECT_EQ(5 * BitDepth::EIGHT, 5);
  // EXPECT_EQ(BitDepth::EIGHT / 4, 0);
  // EXPECT_EQ(3 / BitDepth::EIGHT, 3);

  // EXPECT_FLOAT_EQ(BitDepth::SIXTEEN * 2.f, 4.f);
  // EXPECT_FLOAT_EQ(5.f * BitDepth::SIXTEEN, 10.f);
  // EXPECT_FLOAT_EQ(BitDepth::SIXTEEN / 4.f, 0.5f);
  // EXPECT_FLOAT_EQ(4.f / BitDepth::SIXTEEN, 2.f);

  // EXPECT_DOUBLE_EQ(BitDepth::TWENTYFOUR * 3., 9.);
  // EXPECT_DOUBLE_EQ(6. * BitDepth::TWENTYFOUR, 18.);
  // EXPECT_DOUBLE_EQ(BitDepth::TWENTYFOUR / 8.0, 3.0 / 8.0);
  // EXPECT_DOUBLE_EQ(12. / BitDepth::TWENTYFOUR, 4.);

  // EXPECT_EQ(BitDepth::THIRTYTWO * 4, 16);
  // EXPECT_EQ(7 * BitDepth::THIRTYTWO, 28);
  // EXPECT_EQ(BitDepth::THIRTYTWO / 16, 0);
  // EXPECT_EQ(20 / BitDepth::THIRTYTWO, 5);

  EXPECT_EQ(BitDepth::EIGHT * std::uint8_t{3}, 3);
  EXPECT_EQ(std::uint8_t{4} * BitDepth::EIGHT, 4);
  EXPECT_EQ(BitDepth::EIGHT / std::uint8_t{2}, 0);
  EXPECT_EQ(std::uint8_t{10} / BitDepth::EIGHT, 10);
  EXPECT_EQ(std::uint8_t{0} / BitDepth::EIGHT, 0);

  EXPECT_EQ(BitDepth::EIGHT * 2L, 2L);
  EXPECT_EQ(5L * BitDepth::EIGHT, 5L);
  EXPECT_EQ(BitDepth::EIGHT / 4L, 0L);
  EXPECT_EQ(8L / BitDepth::EIGHT, 8L);
  EXPECT_EQ(0L / BitDepth{6}, 0L);
}