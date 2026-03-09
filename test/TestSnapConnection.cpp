#include "BrilliantSnapcast/Message.hpp"
#include "BrilliantSnapcast/MessageConv.hpp"
#include "FakeSocket.hpp"
#include <BrilliantSnapcast/SnapConnection.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/system/detail/errc.hpp>

struct TestSnapConnection : testing::Test {
  TestSnapConnection()
      : testing::Test(),
        mr(std::pmr::get_default_resource()),
        TcpConnection(
            FakeSocket<boost::asio::ip::tcp>{context.get_executor(), &state},
            mr),
        SnapConnection(TcpConnection) {}

  std::pmr::memory_resource* mr;
  SocketState state;
  boost::asio::io_context context;
  brilliant::snapcast::TcpConnection<FakeSocket<boost::asio::ip::tcp>> TcpConnection;
  brilliant::snapcast::SnapConnection<FakeSocket<boost::asio::ip::tcp>> SnapConnection;
};

TEST_F(TestSnapConnection, testSendTime) {
  boost::asio::co_spawn(
      context,
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
      [this] -> boost::asio::awaitable<void> {
        constexpr auto bufferSize = 4096;
        std::vector<std::byte> buffer(bufferSize);
        auto result = co_await SnapConnection.send(0, brilliant::snapcast::Time{},
                                               std::span(buffer));
        EXPECT_FALSE(result);

        brilliant::snapcast::Base base{};
        brilliant::snapcast::read(std::span(state.outData), base);
        EXPECT_EQ(base.type, brilliant::snapcast::MessageType::TIME);

        auto msg = brilliant::snapcast::read(
            std::span(state.outData)
                .subspan(brilliant::snapcast::BASE_SIZE_BYTES, base.size),
            base.type);
        std::visit(
            [](const auto& m) {
              if constexpr (std::is_same_v<std::decay_t<decltype(m)>,
                                           brilliant::snapcast::Time>) {
                EXPECT_NE(m.sec, 0);
                EXPECT_NE(m.sec, 0);
              } else {
                EXPECT_TRUE(false);
              }
            },
            msg);
      },
      boost::asio::detached);
  context.run();
}

TEST_F(TestSnapConnection, testSendHello) {
  boost::asio::co_spawn(
      context,
      [this] -> boost::asio::awaitable<void> {
        std::vector<std::byte> buffer(1024);
        auto result = co_await SnapConnection.sendHello("mac address", "host field",
                                                    "this os", "xArch", 12,
                                                    std::span(buffer));
        EXPECT_FALSE(result);

        constexpr std::string_view expectedPayload{
            R"({"MAC":"mac address","HostName":"host field","Version":"0.34","ClientName":"SnapConnection","OS":"this os","Arch":"xArch","Instance":"12","ID":"mac address","SnapStreamProtocolVersion":2,"Version":"0.27.0"})"};

        brilliant::snapcast::Base base{};
        // formatted string is stored in the beginning of the buffer so need to
        // offset to where base is written
        brilliant::snapcast::read(std::span(buffer), base);
        EXPECT_EQ(base.type, brilliant::snapcast::MessageType::HELLO);
        EXPECT_EQ(base.size, expectedPayload.size() + 4);

        auto msg = brilliant::snapcast::read(
            std::span(buffer).subspan(brilliant::snapcast::BASE_SIZE_BYTES),
            base.type);
        auto& hello = std::get<brilliant::snapcast::Hello>(msg);
        EXPECT_EQ(hello.size, expectedPayload.size());
        EXPECT_EQ(std::string_view(hello.payload, hello.size), expectedPayload);
      },
      boost::asio::detached);
  context.run();
}

TEST_F(TestSnapConnection, testSendClientInfo) {
  boost::asio::co_spawn(
      context,
      [this] -> boost::asio::awaitable<void> {
        std::vector<std::byte> buffer(1024);
        auto result =
            co_await SnapConnection.sendClientInfo(75, true, std::span(buffer));
        EXPECT_FALSE(result);

        constexpr std::string_view expectedPayload{R"({"volume":75,"muted":true})"};

        brilliant::snapcast::Base base{};
        // formatted string is stored in the beginning of the buffer so need to
        // offset to where base is written
        brilliant::snapcast::read(std::span(buffer), base);
        EXPECT_EQ(base.type, brilliant::snapcast::MessageType::CLIENT_INFO);
        EXPECT_EQ(base.size, expectedPayload.size() + 4);

        auto msg = brilliant::snapcast::read(
            std::span(buffer).subspan(brilliant::snapcast::BASE_SIZE_BYTES),
            base.type);
        auto& info = std::get<brilliant::snapcast::ClientInfo>(msg);
        EXPECT_EQ(info.size, expectedPayload.size());
        EXPECT_EQ(std::string_view(info.payload, info.size), expectedPayload);
      },
      boost::asio::detached);
  context.run();
}

TEST_F(TestSnapConnection, testSendInsufficientBuffer) {
  boost::asio::co_spawn(
      context,
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
      [this] -> boost::asio::awaitable<void> {
        std::vector<std::byte> buffer(brilliant::snapcast::BASE_SIZE_BYTES - 1);

        auto result = co_await SnapConnection.send(0, brilliant::snapcast::Time{},
                                               std::span(buffer));
        EXPECT_TRUE(result);
        EXPECT_EQ(result.value(), boost::system::errc::no_buffer_space);

        buffer.resize(brilliant::snapcast::BASE_SIZE_BYTES + 2);
        result = co_await SnapConnection.send(0, brilliant::snapcast::Time{},
                                          std::span(buffer));
        EXPECT_TRUE(result);
        EXPECT_EQ(result.value(), boost::system::errc::no_buffer_space);
      },
      boost::asio::detached);
  context.run();
}

TEST_F(TestSnapConnection, testSendHelloInsufficientBuffer) {
  boost::asio::co_spawn(
      context,
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
      [this] -> boost::asio::awaitable<void> {
        std::vector<std::byte> buffer(200);

        auto result = co_await SnapConnection.sendHello("mac address", "host field",
                                                    "this os", "xArch", 12,
                                                    std::span(buffer));
        EXPECT_TRUE(result);
        EXPECT_EQ(result.value(), boost::system::errc::no_buffer_space);
      },
      boost::asio::detached);
  context.run();
}

TEST_F(TestSnapConnection, tsetSendClientInfoInsufficientBuffer) {
  boost::asio::co_spawn(
      context,
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
      [this] -> boost::asio::awaitable<void> {
        std::vector<std::byte> buffer(48);

        auto result =
            co_await SnapConnection.sendClientInfo(60, false, std::span(buffer));
        EXPECT_TRUE(result);
        EXPECT_EQ(result.value(), boost::system::errc::no_buffer_space);
      },
      boost::asio::detached);
  context.run();
}

TEST_F(TestSnapConnection, testSendInvalidType) {
  boost::asio::co_spawn(context, [this] -> boost::asio::awaitable<void> {
    std::vector<std::byte> buffer(1024);
    brilliant::snapcast::WireChunk chunk{};
    auto result = co_await SnapConnection.send(0, chunk, std::span(buffer));
    EXPECT_TRUE(result);
    EXPECT_EQ(result.value(), boost::system::errc::invalid_argument);
  }, boost::asio::detached);
  context.run();
}

TEST_F(TestSnapConnection, testRead) {
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

        state.inData.resize(brilliant::snapcast::BASE_SIZE_BYTES +
                            sizeof(brilliant::snapcast::Time));
        brilliant::snapcast::write(std::span(state.inData), base);
        brilliant::snapcast::write(
            std::span(state.inData)
                .subspan(brilliant::snapcast::BASE_SIZE_BYTES,
                         sizeof(brilliant::snapcast::Time)),
            time);

        constexpr auto bufSize = 4096;
        std::vector<std::byte> buffer(bufSize);
        auto result = co_await SnapConnection.read(std::span(buffer));
        EXPECT_TRUE(result.has_value());

        auto& [readBase, msg] = result.value();
        EXPECT_EQ(readBase.type, base.type);
        EXPECT_EQ(readBase.size, base.size);

        auto& readTime = std::get<brilliant::snapcast::Time>(msg);
        EXPECT_EQ(readTime.sec, time.sec);
        EXPECT_EQ(readTime.usec, time.usec);
      },
      boost::asio::detached);
  context.run();
}

TEST_F(TestSnapConnection, testReadInsufficientBuffer) {
  boost::asio::co_spawn(
      context,
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
      [this] -> boost::asio::awaitable<void> {
        std::vector<std::byte> buffer(brilliant::snapcast::BASE_SIZE_BYTES - 1);

        auto result = co_await SnapConnection.read(std::span(buffer));
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error().value(), boost::system::errc::no_buffer_space);

        // NOLINTBEGIN linter complains about magic numbers
        brilliant::snapcast::Base base{};
        base.type = brilliant::snapcast::MessageType::SERVER_SETTINGS;
        base.size = 35;
        state.inData.resize(1234);
        // NOLINTEND
        brilliant::snapcast::write(std::span(state.inData), base);

        buffer.resize(brilliant::snapcast::BASE_SIZE_BYTES + 2);
        result = co_await SnapConnection.read(std::span(buffer));
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error().value(), boost::system::errc::no_buffer_space);
      },
      boost::asio::detached);
  context.run();
}
