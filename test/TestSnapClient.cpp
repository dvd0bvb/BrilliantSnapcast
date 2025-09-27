#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/asio.hpp>

#include "BrilliantSnapcast/SnapClient.hpp"
#include "FakeSocket.hpp"
#include "FakeUtilProvider.hpp"

struct TestSnapClient : testing::Test {
  TestSnapClient()
      : testing::Test(),
        mr(std::pmr::get_default_resource()),
        tcpClient(
            FakeSocket<boost::asio::ip::tcp>{context.get_executor(), &state},
            mr),
        snapClient(tcpClient) {}

  std::pmr::memory_resource* mr;
  SocketState state;
  boost::asio::io_context context;
  brilliant::snapcast::TcpClient<FakeSocket<boost::asio::ip::tcp>> tcpClient;
  brilliant::snapcast::SnapClient<FakeSocket<boost::asio::ip::tcp>> snapClient;
  FakeUtilProvider utilProvider;
  boost::json::serializer serializer;
};

TEST_F(TestSnapClient, testSendTime) {
  boost::asio::co_spawn(
      context,
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
      [this] -> boost::asio::awaitable<void> {
        constexpr auto bufferSize = 4096;
        std::vector<std::byte> buffer(bufferSize);
        auto result = co_await snapClient.send(0, brilliant::snapcast::Time{},
                                               std::span(buffer));
        EXPECT_TRUE(result.has_value());

        const auto sentTime = result.value();

        brilliant::snapcast::Base base{};
        brilliant::snapcast::read(std::span(state.outData), base);
        EXPECT_EQ(base.type, brilliant::snapcast::MessageType::TIME);

        auto msg = brilliant::snapcast::read(
            std::span(state.outData)
                .subspan(sizeof(brilliant::snapcast::Base), base.size),
            base.type);
        std::visit(
            [&sentTime](const auto& m) {
              if constexpr (std::is_same_v<std::decay_t<decltype(m)>,
                                           brilliant::snapcast::Time>) {
                EXPECT_EQ(sentTime.sec, m.sec);
                EXPECT_EQ(sentTime.usec, m.usec);
              } else {
                EXPECT_TRUE(false);
              }
            },
            msg);
      },
      boost::asio::detached);
  context.run();
}

TEST_F(TestSnapClient, testSendJsonMessage) {
  boost::asio::co_spawn(
      context,
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
      [this] -> boost::asio::awaitable<void> {
        constexpr auto bufSize = 4096;
        std::vector<std::byte> buffer(bufSize);
        auto result = co_await snapClient.send(
            0, brilliant::snapcast::Hello{"testing"}, std::span(buffer));
        EXPECT_TRUE(result.has_value());

        brilliant::snapcast::Base base{};
        brilliant::snapcast::read(std::span(state.outData), base);
        EXPECT_EQ(base.type, brilliant::snapcast::MessageType::HELLO);

        auto msg = brilliant::snapcast::read(
            std::span(state.outData)
                .subspan(sizeof(brilliant::snapcast::Base), base.size),
            base.type);
        std::visit(
            [](const auto& m) {
              if constexpr (std::is_same_v<std::decay_t<decltype(m)>,
                                           brilliant::snapcast::Hello>) {
                EXPECT_EQ("testing", std::string_view(m.payload, m.size));
              } else {
                EXPECT_TRUE(false);
              }
            },
            msg);
      },
      boost::asio::detached);
  context.run();
}

TEST_F(TestSnapClient, testSendWireChunk) {
  boost::asio::co_spawn(
      context,
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
      [this] -> boost::asio::awaitable<void> {
        constexpr auto bufSize = 4096;
        std::vector<std::byte> buffer(bufSize);
        // NOLINTBEGIN
        std::vector<std::byte> payload{std::byte{0xde}, std::byte{0xad},
                                       std::byte{0xbe}, std::byte{0xef}};
        // NOLINTEND

        auto result = co_await snapClient.send(
            0, brilliant::snapcast::WireChunk{std::span(payload)},
            std::span(buffer));
        EXPECT_TRUE(result.has_value());

        brilliant::snapcast::Base base{};
        brilliant::snapcast::read(std::span(state.outData), base);
        EXPECT_EQ(base.type, brilliant::snapcast::MessageType::WIRE_CHUNK);

        auto msg = brilliant::snapcast::read(
            std::span(state.outData)
                .subspan(sizeof(brilliant::snapcast::Base), base.size),
            base.type);
        std::visit(
            [&payload](const auto& m) {
              if constexpr (std::is_same_v<std::decay_t<decltype(m)>,
                                           brilliant::snapcast::WireChunk>) {
                EXPECT_THAT(std::span(m.payload, m.size),
                            testing::ElementsAreArray(payload));
              } else {
                EXPECT_TRUE(false);
              }
            },
            msg);
      },
      boost::asio::detached);
  context.run();
}

TEST_F(TestSnapClient, testSendInsufficientBuffer) {
  boost::asio::co_spawn(
      context,
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
      [this] -> boost::asio::awaitable<void> {
        std::vector<std::byte> buffer(sizeof(brilliant::snapcast::Base) - 1);

        auto result = co_await snapClient.send(0, brilliant::snapcast::Time{},
                                               std::span(buffer));
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error().value(), boost::system::errc::no_buffer_space);

        buffer.resize(sizeof(brilliant::snapcast::Base) + 2);
        result = co_await snapClient.send(0, brilliant::snapcast::Time{},
                                          std::span(buffer));
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error().value(), boost::system::errc::no_buffer_space);
      },
      boost::asio::detached);
  context.run();
}

TEST_F(TestSnapClient, testRead) {
  boost::asio::co_spawn(
      context,
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
      [this] -> boost::asio::awaitable<void> {
        // read relies mostly on functions defined in MessageConv.hpp which are
        // tested elsewhere so this author finds extensive read() testing
        // redundant
        brilliant::snapcast::Base base{
            .type = brilliant::snapcast::MessageType::TIME,
            .id = 0,
            .refersTo = 3,
            .sent = brilliant::snapcast::Time{.sec = 1, .usec = 2},
            .received = brilliant::snapcast::Time{},  // received gets set by
                                                      // the function
            .size = sizeof(brilliant::snapcast::Time)};
        const brilliant::snapcast::Time time{.sec = 12, .usec = 34};

        state.inData.resize(sizeof(brilliant::snapcast::Base) +
                            sizeof(brilliant::snapcast::Time));
        brilliant::snapcast::write(std::span(state.inData), base);
        brilliant::snapcast::write(
            std::span(state.inData)
                .subspan(sizeof(brilliant::snapcast::Base),
                         sizeof(brilliant::snapcast::Time)),
            time);

        constexpr auto bufSize = 4096;
        std::vector<std::byte> buffer(bufSize);
        auto result = co_await snapClient.read(std::span(buffer));
        EXPECT_TRUE(result.has_value());

        auto [readBase, msg] = result.value();
        EXPECT_EQ(readBase.type, base.type);
        EXPECT_EQ(readBase.size, base.size);

        auto readTime = std::get<brilliant::snapcast::Time>(msg);
        EXPECT_EQ(readTime.sec, time.sec);
        EXPECT_EQ(readTime.usec, time.usec);
      },
      boost::asio::detached);
  context.run();
}

TEST_F(TestSnapClient, testReadInsufficientBuffer) {
  boost::asio::co_spawn(
      context,
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
      [this] -> boost::asio::awaitable<void> {
        std::vector<std::byte> buffer(sizeof(brilliant::snapcast::Base) - 1);

        auto result = co_await snapClient.read(std::span(buffer));
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error().value(), boost::system::errc::no_buffer_space);

        // NOLINTBEGIN linter complains about magic numbers
        brilliant::snapcast::Base base{};
        base.type = brilliant::snapcast::MessageType::SERVER_SETTINGS;
        base.size = 35;
        state.inData.resize(1234);
        // NOLINTEND
        brilliant::snapcast::write(std::span(state.inData), base);

        buffer.resize(sizeof(brilliant::snapcast::Base) + 2);
        result = co_await snapClient.read(std::span(buffer));
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error().value(), boost::system::errc::no_buffer_space);
      },
      boost::asio::detached);
  context.run();
}
