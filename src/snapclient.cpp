

#include "snapcastpp/snapclient.hpp"
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/registered_buffer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/json/impl/serialize.ipp>
#include <boost/json/serializer.hpp>
#include <boost/json/src.hpp>
#include <boost/json/value.hpp>
#include <boost/url/parse.hpp>
#include <chrono>
#include <variant>
// #include <boost/predef/architecture.h> // possbily use to determine arch
#include "message.hpp"
#include "snapcastpp/message.hpp"

namespace snapcastpp {

SnapClient::SnapClient(const boost::asio::any_io_executor &exec)
    : socket(exec) {}

auto SnapClient::connect(std::string_view uri) -> boost::asio::awaitable<void> {
  if (auto parsed = boost::urls::parse_uri(uri); parsed) {
    // TODO(david): use scheme for websocket/ssl
    // if (parsed->has_scheme()) {
    //   if (parsed->scheme() == "tcp") {

    //   } else if (parsed->scheme() == "ws") {

    //   } else if (parsed->scheme() == "wss") {
    //   }
    // } else {
    // }

    auto exec = co_await boost::asio::this_coro::executor;

    boost::asio::ip::tcp::resolver resolver(exec);
    auto [ec, results] = co_await resolver.async_resolve(
        parsed->host(), parsed->port(),
        boost::asio::as_tuple(boost::asio::use_awaitable));
    // TODO(david): test ec

    for (auto &&r : results) {
      if (auto [ec] = co_await socket.async_connect(
              r.endpoint(), boost::asio::as_tuple(boost::asio::use_awaitable));
          !ec) {
        break;
      } else {
        // TODO(david): log error code
      }
    }
  }
}

auto SnapClient::doHandshake() -> boost::asio::awaitable<void> {
  // send hello
  const std::string macAddress =
      ""; // TODO(david): probably use a util provider class to provide hw
          // specific things <esp_wifi> provides esp_wifi_get_mac()
  boost::json::object helloJson{{"MAC", macAddress},
                                {"HostName", ""},
                                {"Version", "0.32.4"},
                                {"ClientName", "Snapclient"},
                                {"OS", ""},
                                {"Arch", "unknown"},
                                {"Instance", 1},
                                {"ID", macAddress},
                                {"SnapStreamProtocolVersion", 2}};

  Hello hello(0, boost::json::serialize(helloJson));
  auto [ec, bytesSent] = co_await send(std::move(hello));

  // wait for server settings
  auto msg = co_await read();
  // do stuff with server settings
    // if it is actually server settings... 
  // wait for codec header
  msg = co_await read();
  // do stuff with codec header
    // if it is actually a codec header... 
  // ignore wire chunks during this process
}

auto SnapClient::send(Message message) -> boost::asio::awaitable<
    std::tuple<boost::system::error_code, std::size_t>> {
  const auto buffer = std::visit(
      [](auto &msg) {
        const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch());
        const auto nowSecs =
            std::chrono::duration_cast<std::chrono::seconds>(now);
        msg.sent.sec = static_cast<std::uint32_t>(nowSecs.count());
        msg.sent.usec = static_cast<std::uint32_t>(
            static_cast<std::uint32_t>((now - nowSecs).count()));

        // serialize
        return "";
      },
      message);

  return socket.async_send(buffer,
                           boost::asio::as_tuple(boost::asio::use_awaitable));
}

auto SnapClient::read() -> boost::asio::awaitable<Message> {
  std::vector<std::byte>
      b; // TODO(david): this should probably be a member variable. Should also
         // allow the user to decide storage/allocation options
  auto [ec, bytesRead] = co_await socket.async_receive(
      b, boost::asio::as_tuple(boost::asio::use_awaitable));
  // TODO(david): deserialize, return error code if there was an error
  co_return ServerSettings{1, ""};
}

} // namespace snapcastpp