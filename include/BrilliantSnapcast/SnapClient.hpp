
#pragma once

#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <boost/json/src.hpp>
#include <chrono>
#include <cstddef>
#include <expected>
#include <memory_resource>
#include <type_traits>
#include <utility>

#include "BrilliantSnapcast/BoostPmrWrapper.hpp"
#include "BrilliantSnapcast/Message.hpp"
#include "BrilliantSnapcast/MessageConv.hpp"
#include "BrilliantSnapcast/MessageType.hpp"
#include "BrilliantSnapcast/TcpClient.hpp"
#include "BrilliantSnapcast/UtilProvider.hpp"

namespace brilliant::snapcast {

  /**
   * @brief Implements snapcast client functionality
   *
   * @tparam Socket The socket type
   */
  template <class Socket>
  class SnapClient {
  public:
    /**
     * @brief Construct a new Snap Client object
     *
     * @param tcpClient The tcp client used for network operations
     * @param mr A pointer to the memory resource used for dynamic allocations
     */
    SnapClient(TcpClient<Socket>& tcpClient, std::pmr::memory_resource* mr)
        : _tcpClient(&tcpClient), _mr(mr) {}

    /**
     * @brief Construct a new Snap Client object. Uses the memory_resource
     * contained in tcpClient.
     *
     * @param tcpClient The tcp client used for network operations
     */
    SnapClient(TcpClient<Socket>& tcpClient)
        : _tcpClient(&tcpClient), _mr(tcpClient.getAllocator().resource()) {}

    /**
     * @brief Send a message to the server. Creates the message header and
     * populates sent time using std::chrono::steady_clock.
     *
     * @tparam Extent The extent of the buffer
     * @param id The message id
     * @param message The message to send
     * @param buffer The buffer to copy serialized data to. Passed to network
     * calls once populated.
     * @return If the operation was successful, returns a Time struct containing
     * the time that was populated in the outgoing header. This should be used
     * when sending Time messagese to calculate network latency. If the
     * operation fails, an error_code describing the failure is returned.
     */
    template <std::size_t Extent>
    auto send(std::uint16_t id, Message message,
              std::span<std::byte, Extent> buffer)
        -> boost::asio::awaitable<
            std::expected<Time, boost::system::error_code>> {
      auto base = std::visit(
          [this, &buffer, id](auto& msg) {
            using type = std::decay_t<decltype(msg)>;

            const auto now =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now().time_since_epoch());
            const auto nowSecs =
                std::chrono::duration_cast<std::chrono::seconds>(now);

            Base b{};
            b.id = id;
            b.refersTo = 0;  // TODO(david):
            b.sent.sec = static_cast<std::uint32_t>(nowSecs.count());
            b.sent.usec = static_cast<std::uint32_t>((now - nowSecs).count());
            if constexpr (std::is_same_v<type, Hello>) {
              b.type = MessageType::HELLO;
              b.size = static_cast<std::uint32_t>(sizeof(msg.size) + msg.size);
            } else if constexpr (std::is_same_v<type, ClientInfo>) {
              b.type = MessageType::CLIENT_INFO;
              b.size = static_cast<std::uint32_t>(sizeof(msg.size) + msg.size);
            } else if constexpr (std::is_same_v<type, ServerSettings>) {
              b.type = MessageType::SERVER_SETTINGS;
              b.size = static_cast<std::uint32_t>(sizeof(msg.size) + msg.size);
            } else if constexpr (std::is_same_v<type, Time>) {
              b.type = MessageType::TIME;
              b.size = static_cast<std::uint32_t>(sizeof(Time));
              msg = b.sent;
            } else if constexpr (std::is_same_v<type, WireChunk>) {
              b.type = MessageType::WIRE_CHUNK;
              b.size = static_cast<std::uint32_t>(sizeof(Time) +
                                                  sizeof(msg.size) + msg.size);
            } else {
              std::unreachable();
            }
            return b;
          },
          message);

      if (std::size(buffer) < (sizeof(Base) + base.size)) {
        co_return std::unexpected(boost::system::errc::make_error_code(
            boost::system::errc::no_buffer_space));
      }

      brilliant::snapcast::write(buffer.first(sizeof(Base)), base);
      brilliant::snapcast::write(buffer.subspan(sizeof(Base), base.size), message);
      auto [ec, size] =
          co_await _tcpClient->write(buffer.first(sizeof(Base) + base.size));
      if (ec) {
        co_return std::unexpected(ec);
      }
      co_return base.sent;
    }

    /**
     * @brief Convenience function for creating a json message
     *
     * @tparam Extent The buffer extent
     * @param id The message id
     * @param type The message type. Message types for non-json messages are
     * invalid
     * @param object The json object to serialize
     * @param serializer The json serializer
     * @param buffer The buffer to store serialized data. The serialized json
     * string also uses the buffer.
     * @return The time stamp of the sent message if successful, an error_code
     * otherwise.
     */
    template <std::size_t Extent>
    auto sendJson(uint16_t id, MessageType type, boost::json::object& object,
                  boost::json::serializer& serializer,
                  std::span<std::byte, Extent> buffer)
        -> boost::asio::awaitable<
            std::expected<Time, boost::system::error_code>> {
      constexpr std::size_t TMP_BUF_SIZE = 512;

      serializer.reset(&object);
      std::size_t totalSize{};

      std::array<char, TMP_BUF_SIZE> tmpBuf{};
      for (; !serializer.done();) {
        auto sv = serializer.read(tmpBuf.data(), tmpBuf.size());
        std::memcpy(buffer.data() + totalSize, sv.data(), sv.size());
        totalSize += sv.size();
      }

      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      std::string_view jsonString(reinterpret_cast<char*>(std::data(buffer)),
                                  totalSize);

      Message message = [type, jsonString] -> Message {
        switch (type) {
        case MessageType::HELLO:
          return Hello(jsonString);
        case MessageType::CLIENT_INFO:
          return ClientInfo(jsonString);
        case MessageType::SERVER_SETTINGS:
          return ServerSettings(jsonString);
        default:
          std::unreachable();
        }
      }();
      // Ok to return the awaitable, buffer's lifetime outlasts this call
      // and data is either copied/moved (like id or msg.size) or are pointers
      // into buffer
      return send(id, std::move(message), buffer.subspan(totalSize));
    }

    /**
     * @brief Convenience function to send a Hello message. Populates platform
     * dependent information from the UtilProvider instance.
     *
     * @tparam Extent The buffer extent
     * @param utilProvider A reference to the util proivider
     * @param serializer A reference to the json serializer
     * @param buffer The buffer to store serialized data. The serialized json
     * string also uses the buffer.
     * @return The time stamp of the sent message if successful, an error_code
     * otherwise.
     */
    template <std::size_t Extent>
    auto sendHello(UtilProvider& utilProvider,
                   boost::json::serializer& serializer,
                   std::span<std::byte, Extent> buffer)
        -> boost::asio::awaitable<
            std::expected<Time, boost::system::error_code>> {
      const auto macAddress = utilProvider.getMacAddress(0, _mr);
      BoostPmrWrapper wrapper(_mr);
      boost::json::storage_ptr jsonAlloc(&wrapper);
      boost::json::object helloJson({{"MAC", macAddress},
                                     {"HostName", ""},
                                     {"Version", "0.34"},
                                     {"ClientName", "Snapclient"},
                                     {"OS", utilProvider.getOS(_mr)},
                                     {"Arch", utilProvider.getArch(_mr)},
                                     {"Instance", ""},
                                     {"ID", macAddress},
                                     {"SnapStreamProtocolVersion", 2}},
                                    jsonAlloc);
      return sendJson(0, MessageType::HELLO, helloJson, serializer, buffer);
    }

    /**
     * @brief Read a message from the server
     *
     * @tparam Extent The buffer extent
     * @param buffer View of storage to read raw data into
     * @return The message header and message read from the data stream if
     * successful. An error code otherwise.
     */
    template <std::size_t Extent>
    auto read(std::span<std::byte, Extent> buffer)
        -> boost::asio::awaitable<std::expected<std::tuple<Base, Message>,
                                                boost::system::error_code>> {
      if (std::size(buffer) < sizeof(Base)) {
        co_return std::unexpected(boost::system::errc::make_error_code(
            boost::system::errc::no_buffer_space));
      }

      auto [ec, size] = co_await _tcpClient->read(buffer.first(sizeof(Base)));
      if (ec) {
        co_return std::unexpected(ec);
      }

      const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now().time_since_epoch());
      const auto nowSecs =
          std::chrono::duration_cast<std::chrono::seconds>(now);

      Base base{};
      brilliant::snapcast::read(buffer, base);
      if (std::size(buffer) < base.size) {
        co_return std::unexpected(boost::system::errc::make_error_code(
            boost::system::errc::no_buffer_space));
      }

      std::tie(ec, size) = co_await _tcpClient->read(buffer.first(base.size));
      if (ec) {
        co_return std::unexpected(ec);
      }

      base.received.sec = static_cast<std::uint32_t>(nowSecs.count());
      base.received.usec = static_cast<std::uint32_t>((now - nowSecs).count());
      co_return std::make_tuple(base, brilliant::snapcast::read(buffer, base.type));
    }

  private:
    /// Pointer to the tcp client
    TcpClient<Socket>* _tcpClient;

    /// Pointer to the memory resource
    std::pmr::memory_resource* _mr;
  };

}  // namespace brilliant::snapcast
