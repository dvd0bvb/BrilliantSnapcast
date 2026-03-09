#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <BrilliantSnapcast/TcpConnection.hpp>
#include "FakeSocket.hpp"

class TestTcpConnection : public testing::Test {
public:
  auto makeTcpConnection()
      -> brilliant::snapcast::TcpConnection<FakeSocket<boost::asio::ip::tcp>> {
    return {
        FakeSocket<boost::asio::ip::tcp>(context.get_executor(), &socketState),
        mr};
  }

  boost::asio::io_context context;
  SocketState socketState;
  std::pmr::memory_resource* mr = std::pmr::get_default_resource();
};

TEST_F(TestTcpConnection, testConnect) {
  auto TcpConnection = makeTcpConnection();
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
  boost::asio::co_spawn(
      context,
      [this, &TcpConnection] -> boost::asio::awaitable<void> {
        constexpr auto port = 1234;

        // simulate failed connect
        socketState.ec = boost::asio::error::connection_refused;
        {
          auto ep = boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("10.0.0.1"), port);
          auto ec = co_await TcpConnection.connect(ep);
          EXPECT_EQ(ec.value(), static_cast<int>(boost::asio::error::connection_refused));
          EXPECT_FALSE(TcpConnection.isConnected());
        }

        socketState.ec = boost::system::error_code{};
        {
          auto ep = boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("192.168.0.1"), port);
          auto ec = co_await TcpConnection.connect(ep);
          EXPECT_FALSE(ec);
          EXPECT_TRUE(TcpConnection.isConnected());
        }
      },
      boost::asio::detached);
  context.run();

  TcpConnection.disconnect();
  EXPECT_FALSE(TcpConnection.isConnected());
}

TEST_F(TestTcpConnection, testRead) {
  auto TcpConnection = makeTcpConnection();
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
  boost::asio::co_spawn(
      context,
      [this, &TcpConnection] -> boost::asio::awaitable<void> {
        std::string_view data("testing");
        socketState.inData.resize(data.size());
        std::memcpy(socketState.inData.data(), data.data(), data.size());

        constexpr auto bufSize = 4096;
        std::vector<std::byte> buffer(bufSize);

        socketState.ec = boost::asio::error::operation_aborted;
        {
          auto res = co_await TcpConnection.read(std::span(buffer.data(), data.size()));
          EXPECT_FALSE(res.has_value());
          EXPECT_EQ(res.error().value(), static_cast<int>(boost::asio::error::operation_aborted));
        }

        socketState.ec = boost::system::error_code{};
        {
          auto res = co_await TcpConnection.read(std::span(buffer.data(), data.size()));
          EXPECT_TRUE(res.has_value());
          EXPECT_EQ(res.value(), data.size());
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        EXPECT_EQ(data,
                  std::string_view(reinterpret_cast<const char*>(buffer.data()),
                                   data.size()));
      },
      boost::asio::detached);
  context.run();
}

TEST_F(TestTcpConnection, testWrite) {
  auto TcpConnection = makeTcpConnection();
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
  boost::asio::co_spawn(
      context,
      [this, &TcpConnection] -> boost::asio::awaitable<void> {
        std::string_view data("hello test");

        constexpr auto bufSize = 4096;
        std::vector<std::byte> buffer(bufSize);
        std::memcpy(buffer.data(), data.data(), data.size());

        socketState.ec = boost::asio::error::no_such_device;
        {
          auto res = co_await TcpConnection.write(std::span(buffer.data(), data.size()));
          EXPECT_FALSE(res.has_value());
          EXPECT_EQ(res.error().value(), static_cast<int>(boost::asio::error::no_such_device));
        }

        socketState.ec = boost::system::error_code{};
        {
          auto res = co_await TcpConnection.write(std::span(buffer.data(), data.size()));
          EXPECT_TRUE(res.has_value());
          EXPECT_EQ(res.value(), data.size());
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        EXPECT_EQ(data, std::string_view(reinterpret_cast<const char*>(
                           socketState.outData.data()),
                         data.size()));
      },
      boost::asio::detached);
  context.run();
}
