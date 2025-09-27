
#pragma once

#include "message.hpp"
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/detail/error_code.hpp>
#include <optional>
#include <string_view>

namespace snapcastpp {

class SnapClient {
public:
  SnapClient(const boost::asio::any_io_executor &exec);

  auto connect(std::string_view uri) -> boost::asio::awaitable<void>;

  auto doHandshake() -> boost::asio::awaitable<void>;

  // TODO(david): message queue, send queue as generators? or have users use
  // callbacks for commands(volume, etc)/wire chunks?

  auto send(Message message) -> boost::asio::awaitable<
      std::tuple<boost::system::error_code, std::size_t>>;

  auto read() -> boost::asio::awaitable<Message>;

private:
  boost::asio::ip::tcp::socket socket;
};

} // namespace snapcastpp
