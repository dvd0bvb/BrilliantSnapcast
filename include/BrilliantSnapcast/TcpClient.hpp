#pragma once

#include <memory_resource>
#include <span>
#include <string_view>
#include <boost/asio.hpp>

namespace brilliant::snapcast {

  /**
   * @brief Encapsulates network calls for a TCP connection
   *
   * @tparam Socket The socket type
   */
  template <class Socket>
  class TcpClient {
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
    TcpClient(Socket socket, std::pmr::memory_resource* mr)
        : _socket(std::move(socket)), _alloc(mr) {}

    /**
     * @brief Destroy the Tcp Client object. If the socket is open
     * it will be closed on destruction.
     */
    ~TcpClient() {
      if (isConnected()) {
        disconnect();
      }
    }

    /**
     * @brief Deleted copy constructor
     *
     */
    TcpClient(const TcpClient&) = delete;

    /**
     * @brief Deleted copy assignment operator
     *
     * @return TcpClient&
     */
    auto operator=(const TcpClient&) -> TcpClient& = delete;

    /**
     * @brief Move constructor
     *
     * @param other The object to move from
     */
    TcpClient(TcpClient&& other) noexcept
        : TcpClient(std::move(other._socket), other._alloc.resource()) {}

    /**
     * @brief Move assignment operator
     *
     * @param other The object to move from
     * @return A reference to this object
     */
    auto operator=(TcpClient&& other) noexcept -> TcpClient& {
      disconnect();
      _socket = std::move(other._socket);
    }

    /**
     * @brief Connect to a listener
     *
     * @param ip The ip of the remote endpoint
     * @param port The port of the remote endpoint
     * @return An error_code. Empty if the operation was successful.
     */
    auto connect(std::string_view ip, boost::asio::ip::port_type port)
        -> boost::asio::awaitable<boost::system::error_code> {
      boost::system::error_code ec{};
      // copy to string to guarantee trailing 0
      std::pmr::string ipStr(ip.data(), ip.size(), _alloc);
      auto address = boost::asio::ip::make_address(ipStr.c_str(), ec);
      if (ec) {
        co_return ec;
      }

      auto allocatorBoundHandler =
          boost::asio::bind_allocator(_alloc, boost::asio::use_awaitable);
      typename protocol::endpoint ep(address, port);
      std::tie(ec) = co_await _socket.async_connect(
          ep, boost::asio::as_tuple(allocatorBoundHandler));
      co_return ec;
    }

    /**
     * @brief Disconnect from the server
     *
     */
    void disconnect() { _socket.close(); }

    /**
     * @brief Check if the socket is open
     *
     * @return True if the socket is open
     */
    [[nodiscard]] auto isConnected() const -> bool { return _socket.is_open(); }

    /**
     * @brief Read data into a buffer
     *
     * @tparam Extent The extent of the buffer
     * @param buffer The buffer to read into
     * @return An error_code and the number of bytes read. The error_code is
     * empty if the operation was successful.
     */
    template <std::size_t Extent>
    auto read(std::span<std::byte, Extent> buffer)
        -> boost::asio::awaitable<
            std::tuple<boost::system::error_code, std::size_t>> {
      auto handler =
          boost::asio::bind_allocator(_alloc, boost::asio::use_awaitable);
      return boost::asio::async_read(_socket, boost::asio::buffer(buffer),
                                     boost::asio::as_tuple(handler));
    }

    /**
     * @brief Write
     *
     * @tparam Extent
     * @param buffer
     * @return An error_code and the number of bytes read. The error_code is
     * empty if the operation was successful.
     */
    template <std::size_t Extent>
    auto write(std::span<std::byte, Extent> buffer)
        -> boost::asio::awaitable<
            std::tuple<boost::system::error_code, std::size_t>> {
      auto handler =
          boost::asio::bind_allocator(_alloc, boost::asio::use_awaitable);
      return boost::asio::async_write(_socket, boost::asio::buffer(buffer),
                                      boost::asio::as_tuple(handler));
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