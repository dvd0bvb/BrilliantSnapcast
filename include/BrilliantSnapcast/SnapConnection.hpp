
#pragma once

#include <BrilliantSnapcast/Message.hpp>
#include <BrilliantSnapcast/MessageConv.hpp>
#include <BrilliantSnapcast/TcpConnection.hpp>
#include <boost/system/detail/errc.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/errc.hpp>
#include <chrono>
#include <cstddef>
#include <expected>
#include <iterator>
#include <span>
#include <spanstream>
#include <system_error>
#include <type_traits>

namespace brilliant::snapcast {

  /**
   * @brief Implements snapcast client functionality
   *
   * @tparam Socket The socket type
   */
  template <class Socket>
  class SnapConnection {
  public:
    /**
     * @brief Construct a new Snap Client object
     *
     * @param TcpConnection The tcp client used for network operations
     * @param mr A pointer to the memory resource used for dynamic allocations
     */
    SnapConnection(TcpConnection<Socket>& tcpConnection,
                   std::pmr::memory_resource* mr)
        : _tcpConnection(&tcpConnection), _mr(mr) {}

    /**
     * @brief Construct a new Snap Client object. Uses the memory_resource
     * contained in TcpConnection.
     *
     * @param TcpConnection The tcp client used for network operations
     */
    SnapConnection(TcpConnection<Socket>& tcpConnection)
        : _tcpConnection(&tcpConnection),
          _mr(tcpConnection.getAllocator().resource()) {}

    /**
     * @brief Send a message to the server. Creates the message header and
     * populates sent time using std::chrono::steady_clock.
     *
     * @tparam Extent The extent of the buffer
     * @param id The message id
     * @param message The message to send
     * @param buffer The buffer to copy serialized data to. Passed to network
     * calls once populated.
     * @return If the operation fails, an error_code describing the failure is
     * returned. Otherwise an empty error_code is returned.
     */
    template <std::size_t Extent>
    auto send(std::uint16_t id, Message message,
              std::span<std::byte, Extent> buffer)
        -> boost::asio::awaitable<std::error_code> {
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
            } else if constexpr (std::is_same_v<type, Time>) {
              b.type = MessageType::TIME;
              b.size = static_cast<std::uint32_t>(sizeof(Time));
              msg = b.sent;
            }
            return b;
          },
          message);

      if (base.type == MessageType::BASE) {
        co_return std::make_error_code(std::errc::invalid_argument);
      } else if (std::size(buffer) < (BASE_SIZE_BYTES + base.size)) {
        co_return std::make_error_code(std::errc::no_buffer_space);
      }

      brilliant::snapcast::write(buffer.first(BASE_SIZE_BYTES), base);
      brilliant::snapcast::write(buffer.subspan(BASE_SIZE_BYTES, base.size),
                                 message);
      if (auto result = co_await _tcpConnection->write(
              buffer.first(BASE_SIZE_BYTES + base.size))) {
        co_return std::error_code{};
      } else {
        co_return result.error();
      }
    }

    /**
     * @brief Convenience function to send a Hello message.
     *
     * The formatted json string uses the provided buffer for storage. This
     * allows us to return the send coroutine object directly instead of
     * co_awaiting inside of this method, as the buffer (and thus the string)
     * must outlive the coroutine.
     *
     * @tparam Extent The buffer extent
     * @param mac MAC address as a string
     * @param host The host name
     * @param os The operating system string
     * @param arch Platform architecture string
     * @param buffer The buffer to use for operations
     * @return If the operation fails, an error_code describing the failure is
     * returned. Otherwise an empty error_code is returned.
     */
    template <std::size_t Extent>
    auto sendHello(std::string_view mac, std::string_view host,
                   std::string_view os, std::string_view arch,
                   std::int32_t instance, std::span<std::byte, Extent> buffer)
        -> boost::asio::awaitable<std::error_code> {
      // 256 for json string + size of base + size field for Hello message
      constexpr auto MIN_BUFFER_SIZE =
          256uz + BASE_SIZE_BYTES + sizeof(Hello::size);
      static_assert(Extent >= MIN_BUFFER_SIZE || Extent == std::dynamic_extent);

      if (buffer.size() < MIN_BUFFER_SIZE) {
        co_return std::make_error_code(std::errc::no_buffer_space);
      }
      // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
      std::ospanstream ostream(
          std::span<char>(reinterpret_cast<char*>(buffer.data()), buffer.size())
              // offset by size of Base and Hello fields
              .subspan(BASE_SIZE_BYTES + sizeof(Hello::size)));
      // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
      std::print(
          ostream,
          R"({{"MAC":"{}","HostName":"{}","Version":"0.34","ClientName":"SnapConnection","OS":"{}","Arch":"{}","Instance":"{}","ID":"{}","SnapStreamProtocolVersion":2,"Version":"0.27.0"}})",
          mac, host, os, arch, instance, mac);
      const auto strSpan = ostream.span();
      // strSpan points into the area of buffer where the Hello json string
      // would be copied anyway
      co_return co_await send(
          0, Hello(std::string_view(strSpan.data(), strSpan.size())), buffer);
    }

    template <std::size_t Extent>
    auto sendClientInfo(std::uint32_t volume, bool muted,
                        std::span<std::byte, Extent> buffer)
        -> boost::asio::awaitable<std::error_code> {
      // Enough space to have 3 digits in volume and false in muted field plus
      // size of Base and ClientInfo fields
      constexpr std::size_t MIN_BUFFER_SIZE = 64;
      static_assert(MIN_BUFFER_SIZE > Extent || Extent == std::dynamic_extent);

      if (buffer.size() < MIN_BUFFER_SIZE) {
        co_return std::make_error_code(std::errc::no_buffer_space);
      }

      // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
      std::ospanstream ostream(
          std::span<char>(reinterpret_cast<char*>(buffer.data()), buffer.size())
              // offset by size of Base and ClientInfo fields
              .subspan(BASE_SIZE_BYTES + sizeof(ClientInfo::size)));
      // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
      std::print(ostream, R"({{"volume":{},"muted":{}}})", volume, muted);
      const auto strSpan = ostream.span();
      co_return co_await send(
          0, ClientInfo(std::string_view(strSpan.data(), strSpan.size())),
          buffer);
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
    auto read(std::span<std::byte, Extent> buffer) -> boost::asio::awaitable<
        std::expected<std::tuple<Base, Message>, std::error_code>> {
      if (std::size(buffer) < BASE_SIZE_BYTES) {
        co_return std::unexpected(
            std::make_error_code(std::errc::no_buffer_space));
      }

      auto result =
          co_await _tcpConnection->read(buffer.first(BASE_SIZE_BYTES));
      if (!result) {
        co_return std::unexpected(result.error());
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

      result = co_await _tcpConnection->read(buffer.first(base.size));
      if (!result) {
        co_return std::unexpected(result.error());
      }

      base.received.sec = static_cast<std::uint32_t>(nowSecs.count());
      base.received.usec = static_cast<std::uint32_t>((now - nowSecs).count());
      co_return std::make_tuple(base,
                                brilliant::snapcast::read(buffer, base.type));
    }

  private:
    /// Pointer to the tcp client
    TcpConnection<Socket>* _tcpConnection;

    /// Pointer to the memory resource
    std::pmr::memory_resource* _mr;
  };

}  // namespace brilliant::snapcast
