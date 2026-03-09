#pragma once

#include <boost/asio.hpp>
#include <expected>
#include <memory_resource>
#include <span>
#include <string_view>
#include <system_error>

namespace brilliant::snapcast {

  /**
   * @brief Encapsulates network calls for a TCP connection
   *
   * @tparam Socket The socket type
   */
  template <class Socket>
  class TcpConnection {
  public:
    /// Type alias for the socket type
    using socket_type = Socket;

    /// Type alias for the protocol
    using protocol = typename socket_type::protocol_type;

    /**
     * @brief Construct a new Tcp Client object
     *
     * @param socket The network socket
     * @param mr A pointer to the memory resource
     */
    TcpConnection(Socket socket, std::pmr::memory_resource* mr)
        : _socket(std::move(socket)), _alloc(mr) {}

    /**
     * @brief Destroy the Tcp Client object. If the socket is open
     * it will be closed on destruction.
     */
    ~TcpConnection() {
      if (isConnected()) {
        disconnect();
      }
    }

    /**
     * @brief Deleted copy constructor
     *
     */
    TcpConnection(const TcpConnection&) = delete;

    /**
     * @brief Deleted copy assignment operator
     *
     * @return TcpConnection&
     */
    auto operator=(const TcpConnection&) -> TcpConnection& = delete;

    /**
     * @brief Move constructor
     *
     * @param other The object to move from
     */
    TcpConnection(TcpConnection&& other) noexcept
        : TcpConnection(std::move(other._socket), other._alloc.resource()) {}

    /**
     * @brief Move assignment operator
     *
     * @param other The object to move from
     * @return A reference to this object
     */
    auto operator=(TcpConnection&& other) noexcept -> TcpConnection& {
      disconnect();
      _socket = std::move(other._socket);
      return *this;
    }

    /**
     * @brief Connect to a listener
     *
     * @param ip The ip of the remote endpoint
     * @param port The port of the remote endpoint
     * @return An error_code. Empty if the operation was successful.
     */
    auto connect(protocol::endpoint ep)
        -> boost::asio::awaitable<std::error_code> {
      boost::system::error_code ec{};
      auto allocatorBoundHandler =
          boost::asio::bind_allocator(_alloc, boost::asio::use_awaitable);
      std::tie(ec) = co_await _socket.async_connect(
          ep, boost::asio::as_tuple(allocatorBoundHandler));
      _socket.set_option(boost::asio::ip::tcp::no_delay(true));
      co_return std::error_code(ec);
    }

    /**
     * @brief Disconnect from the server
     *
     */
    void disconnect() { 
      boost::system::error_code ec{};
      // Avoid throwing
      _socket.close(ec); 
    }

    /**
     * @brief Check if the socket is open
     *
     * @return True if the socket is open
     */
    [[nodiscard]] auto isConnected() const -> bool { return _socket.is_open(); }

    /**
     * @brief Read data into a buffer from the stream
     *
     * @tparam Extent The extent of the buffer
     * @param buffer The buffer to read into
     * @return An error_code and the number of bytes read. The error_code is
     * empty if the operation was successful.
     */
    template <std::size_t Extent>
    auto read(std::span<std::byte, Extent> buffer)
        -> boost::asio::awaitable<
            std::expected<std::size_t, std::error_code>> {
      auto handler =
          boost::asio::bind_allocator(_alloc, boost::asio::use_awaitable);
      auto [ec, read] = co_await boost::asio::async_read(_socket, boost::asio::buffer(buffer.data(), buffer.size()),
                                     boost::asio::as_tuple(handler));
      if (ec) {
        co_return std::unexpected(std::error_code(ec));
      }
      co_return read;
    }

    /**
     * @brief Write data from a buffer to the stream
     *
     * @tparam Extent
     * @param buffer
     * @return An error_code and the number of bytes read. The error_code is
     * empty if the operation was successful.
     */
    template <std::size_t Extent>
    auto write(std::span<std::byte, Extent> buffer)
        -> boost::asio::awaitable<
            std::expected<std::size_t, std::error_code>> {
      auto handler =
          boost::asio::bind_allocator(_alloc, boost::asio::use_awaitable);
      auto [ec, written] = co_await boost::asio::async_write(_socket, boost::asio::buffer(buffer.data(), buffer.size()),
                                      boost::asio::as_tuple(handler));
      if (ec) {
        co_return std::unexpected(std::error_code(ec));
      }
      co_return written;
    }

    /**
     * @brief Get the Allocator object
     *
     * @return A reference to the allocator used for async operations
     */
    [[nodiscard]] auto getAllocator()
        -> std::pmr::polymorphic_allocator<void>& {
      return _alloc;
    }

    /**
     * @brief Get the Allocator object
     *
     * @return A reference to the allocator used for async operations
     */
    [[nodiscard]] auto getAllocator() const
        -> const std::pmr::polymorphic_allocator<void>& {
      return _alloc;
    }

  private:
    /// The socket used for network operations
    Socket _socket;

    /// The allocator used for async operations
    std::pmr::polymorphic_allocator<void> _alloc;
  };

}  // namespace brilliant::snapcast