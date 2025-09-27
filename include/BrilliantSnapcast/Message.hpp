#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <variant>
#include "BrilliantSnapcast/MessageType.hpp"

namespace brilliant::snapcast {

#pragma pack(1)

  /**
   * @brief Time message. Stores a time point as seconds and microseconds
   *
   */
  struct Time {
    /// Seconds value
    std::uint32_t sec;

    /// Microseconds value
    std::uint32_t usec;
  };

  /**
   * @brief Base message struct. Functionally a header for other message types
   *
   */
  struct Base {
    /// The message type
    MessageType type;

    /// Message id
    std::uint16_t id;

    /// Id of the message this message refers to
    std::uint16_t refersTo;

    /// Time the message was sent
    Time sent;

    /// Time the message was received. Only populated when message is received
    Time received;

    /// Size of the message following Base. Does not include the size of Base
    std::uint32_t size;
  };

#pragma pack()

  /**
   * @brief Base class for messages containing only json data. Stores a view of
   * the data so a string this refers to must outlive this object.
   *
   */
  struct JsonMessage {
    /**
     * @brief Construct a new Json Message object
     *
     */
    JsonMessage() = default;

    /**
     * @brief Construct a new Json Message object
     *
     * @param p String representation of the json data to store
     */
    JsonMessage(std::string_view p)
        : payload(p.data()), size(static_cast<std::uint32_t>(p.size())) {}

    /// Pointer to the json string
    const char* payload;

    /// Size of the json string
    std::uint32_t size;
  };

  /**
   * @brief Hello message. Sent to the server upon client connection.
   *
   */
  struct Hello : JsonMessage {
    /**
     * @brief Construct a new Hello object
     *
     */
    Hello() = default;

    /**
     * @brief Construct a new Hello object
     *
     * @param p A view of the json string
     */
    Hello(std::string_view p) : JsonMessage(p) {}
  };

  /**
   * @brief Server settings message. Sent to the client in reply to a Hello
   * message
   *
   */
  struct ServerSettings : JsonMessage {
    /**
     * @brief Construct a new Server Settings object
     *
     */
    ServerSettings() = default;

    /**
     * @brief Construct a new Server Settings object
     *
     * @param p A view of the json string
     */
    ServerSettings(std::string_view p) : JsonMessage(p) {}
  };

  /**
   * @brief Client info message
   *
   */
  struct ClientInfo : JsonMessage {
    /**
     * @brief Construct a new Client Info object
     *
     */
    ClientInfo() = default;

    /**
     * @brief Construct a new Client Info object
     *
     * @param p A view of the json string
     */
    ClientInfo(std::string_view p) : JsonMessage(p) {}
  };

  /**
   * @brief Wire chunk message. Contains encoded audio in the payload and a
   * timestamp for when the audio should be scheduled.
   *
   */
  struct WireChunk {
    /**
     * @brief Construct a new Wire Chunk object
     *
     */
    WireChunk() = default;

    /**
     * @brief Construct a new Wire Chunk object
     *
     * @tparam Extent The span extent
     * @param p The payload. A view into a contiguous range of bytes
     */
    template <std::size_t Extent>
    WireChunk(std::span<std::byte, Extent> p)
        : timestamp{},
          payload(p.data()),
          size(static_cast<std::uint32_t>(p.size())) {}

    /// The time the audio data is scheduled for
    Time timestamp{};

    /// A pointer to the encoded data
    const std::byte* payload{};

    /// The size of the payload
    std::uint32_t size{};
  };

  /**
   * @brief Codec header. Sent to the client in reply to a Hello message
   *
   */
  struct CodecHeader {
    /**
     * @brief Construct a new Codec Header object
     *
     */
    CodecHeader() = default;

    /**
     * @brief Construct a new Codec Header object
     *
     * @tparam Extent The span extent
     * @param c A view of the codec header string
     * @param p A view of the codec payload
     */
    template <std::size_t Extent>
    CodecHeader(std::string_view c, std::span<std::byte, Extent> p)
        : codecSize(static_cast<std::uint32_t>(c.size())),
          size(static_cast<std::uint32_t>(p.size())),
          codec(c.data()),
          payload(p.data()) {}

    /// The size of the codec string
    std::uint32_t codecSize;

    /// The size of the codec payload
    std::uint32_t size;

    /// A pointer to the codec string
    const char* codec;

    /// A pointer to the codec payload
    const std::byte* payload;
  };

  /**
   * @brief Error message. Contains an error code, string and message.
   *
   */
  struct Error {
    /**
     * @brief Construct a new Error object
     *
     */
    Error() = default;

    /**
     * @brief Construct a new Error object
     *
     * @param ec The error code
     * @param err A view of the error string
     * @param errMsg A view of the error message string
     */
    Error(uint32_t ec, std::string_view err, std::string_view errMsg)
        : errorCode(ec),
          errorSize(static_cast<std::uint32_t>(err.size())),
          error(err.data()),
          errorMessageSize(static_cast<std::uint32_t>(errMsg.size())),
          errorMessage(errMsg.data()) {}

    /// The error code
    std::uint32_t errorCode;

    /// The error string size
    std::uint32_t errorSize;

    /// A pointer to the error string
    const char* error;

    /// The error message size
    std::uint32_t errorMessageSize;

    /// A pointer to the error message string
    const char* errorMessage;
  };

  /// Type alias for a variant of message types
  using Message = std::variant<Hello, ServerSettings, ClientInfo, Time,
                               WireChunk, CodecHeader, Error>;

}  // namespace brilliant::snapcast