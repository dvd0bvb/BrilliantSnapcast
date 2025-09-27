#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "BrilliantSnapcast/TcpClient.hpp"
#include "FakeSocket.hpp"

class TestTcpClient : public testing::Test {
public:
  auto makeTcpClient()
      -> brilliant::snapcast::TcpClient<FakeSocket<boost::asio::ip::tcp>> {
    return {
        FakeSocket<boost::asio::ip::tcp>(context.get_executor(), &socketState),
        mr};
  }

  boost::asio::io_context context;
  SocketState socketState;
  std::pmr::memory_resource* mr = std::pmr::get_default_resource();
};

TEST_F(TestTcpClient, testConnect) {
  auto tcpClient = makeTcpClient();
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
  boost::asio::co_spawn(
      context,
      [this, &tcpClient] -> boost::asio::awaitable<void> {
        constexpr auto port = 1234;

        // not an ip address
        auto ec = co_await tcpClient.connect("localhost", port);
        EXPECT_EQ(ec, boost::asio::error::invalid_argument);

        // simulate failed connect
        socketState.ec = boost::asio::error::connection_refused;
        ec = co_await tcpClient.connect("10.0.0.1", port);
        EXPECT_EQ(ec, boost::asio::error::connection_refused);

        socketState.ec = boost::system::error_code{};
        ec = co_await tcpClient.connect("192.168.0.1", port);
        EXPECT_FALSE(ec);
      },
      boost::asio::detached);
  context.run();
}

TEST_F(TestTcpClient, testRead) {
  auto tcpClient = makeTcpClient();
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
  boost::asio::co_spawn(
      context,
      [this, &tcpClient] -> boost::asio::awaitable<void> {
        std::string_view data("testing");
        socketState.inData.resize(data.size());
        std::memcpy(socketState.inData.data(), data.data(), data.size());

        constexpr auto bufSize = 4096;
        std::vector<std::byte> buffer(bufSize);

        socketState.ec = boost::asio::error::operation_aborted;
        auto [ec, size] =
            co_await tcpClient.read(std::span(buffer.data(), data.size()));
        EXPECT_EQ(ec, boost::asio::error::operation_aborted);

        socketState.ec = boost::system::error_code{};
        std::tie(ec, size) =
            co_await tcpClient.read(std::span(buffer.data(), data.size()));
        EXPECT_FALSE(ec);
        EXPECT_EQ(size, data.size());
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        EXPECT_EQ(data,
                  std::string_view(reinterpret_cast<const char*>(buffer.data()),
                                   size));
      },
      boost::asio::detached);
  context.run();
}

TEST_F(TestTcpClient, testWrite) {
  auto tcpClient = makeTcpClient();
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
  boost::asio::co_spawn(
      context,
      [this, &tcpClient] -> boost::asio::awaitable<void> {
        std::string_view data("hello test");

        constexpr auto bufSize = 4096;
        std::vector<std::byte> buffer(bufSize);
        std::memcpy(buffer.data(), data.data(), data.size());

        socketState.ec = boost::asio::error::no_such_device;
        auto [ec, size] =
            co_await tcpClient.write(std::span(buffer.data(), data.size()));
        EXPECT_EQ(ec, boost::asio::error::no_such_device);

        socketState.ec = boost::system::error_code{};
        std::tie(ec, size) =
            co_await tcpClient.write(std::span(buffer.data(), data.size()));
        EXPECT_FALSE(ec);
        EXPECT_EQ(size, data.size());
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        EXPECT_EQ(data, std::string_view(reinterpret_cast<const char*>(
                                             socketState.outData.data()),
                                         size));
      },
      boost::asio::detached);
  context.run();
}
