#pragma once

#include <concepts>
#include <cstring>
#include <span>
#include <type_traits>
#include <utility>

#include "BrilliantSnapcast/Message.hpp"

namespace brilliant::snapcast {

  template <std::size_t Extent>
  void read(std::span<std::byte, Extent> buffer, Time& time) {
    static_assert(Extent == std::dynamic_extent || Extent >= sizeof(Time));

    auto data = buffer.data();
    std::memcpy(&time, data, sizeof(Time));
  }

  template <std::size_t Extent>
  void read(std::span<std::byte, Extent> buffer, Base& base) {
    static_assert(Extent == std::dynamic_extent || Extent >= sizeof(Base));

    auto data = buffer.data();
    std::memcpy(&base, data, sizeof(Base));
  }

  template <class T, std::size_t Extent>
  void read(std::span<std::byte, Extent> buffer, T& t)
    requires std::derived_from<T, JsonMessage>
  {
    std::memcpy(&t.size, buffer.data(), sizeof(t.size));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    t.payload = reinterpret_cast<char*>(buffer.data() + sizeof(t.size));
  }

  template <std::size_t Extent>
  auto read(std::span<std::byte, Extent> buffer, MessageType type) -> Message {
    switch (type) {
    case MessageType::HELLO: {
      Hello hello{};
      read(buffer, hello);
      return hello;
    }
    case MessageType::SERVER_SETTINGS: {
      ServerSettings settings{};
      read(buffer, settings);
      return settings;
    }
    case MessageType::CLIENT_INFO: {
      ClientInfo info{};
      read(buffer, info);
      return info;
    }
    case MessageType::TIME: {
      Time time{};
      read(buffer, time);
      return time;
    }
    case MessageType::WIRE_CHUNK: {
      WireChunk chunk{};
      read(buffer, chunk.timestamp);
      auto data = buffer.data() + sizeof(chunk.timestamp);
      std::memcpy(&chunk.size, data, sizeof(chunk.size));
      chunk.payload = data + sizeof(chunk.size);
      return chunk;
    }
    case MessageType::CODEC_HEADER: {
      CodecHeader header{};
      auto data = buffer.data();
      std::memcpy(&header.codecSize, data, sizeof(header.codecSize));
      data += sizeof(header.codecSize);
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      header.codec = reinterpret_cast<char*>(data);
      data += header.codecSize;
      std::memcpy(&header.size, data, sizeof(header.size));
      data += sizeof(header.size);
      header.payload = data;
      return header;
    }
    case MessageType::ERROR: {
      Error error{};
      auto data = buffer.data();
      std::memcpy(&error.errorCode, data, sizeof(error.errorCode));
      data += sizeof(error.errorCode);
      std::memcpy(&error.errorSize, data, sizeof(error.errorSize));
      data += sizeof(error.errorSize);
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      error.error = reinterpret_cast<char*>(data);
      data += error.errorSize;
      std::memcpy(&error.errorMessageSize, data,
                  sizeof(error.errorMessageSize));
      data += sizeof(error.errorMessageSize);
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      error.errorMessage = reinterpret_cast<char*>(data);
      return error;
    }
    case MessageType::BASE:
    default:
      std::unreachable();
    }
    std::unreachable();
  }

  template <std::size_t Extent>
  void write(std::span<std::byte, Extent> buffer, const Time& time) {
    static_assert(Extent == std::dynamic_extent || Extent >= sizeof(Time));
    std::memcpy(buffer.data(), &time, sizeof(Time));
  }

  template <std::size_t Extent>
  void write(std::span<std::byte, Extent> buffer, const Base& base) {
    static_assert(Extent == std::dynamic_extent || Extent >= sizeof(Time));
    std::memcpy(buffer.data(), &base, sizeof(Base));
  }

  template <std::size_t Extent>
  void write(std::span<std::byte, Extent> buffer, const Message& message) {
    std::visit(
        [buffer](auto& msg) {
          using type = std::decay_t<decltype(msg)>;
          if constexpr (std::derived_from<type, JsonMessage>) {
            auto data = buffer.data();
            std::memcpy(data, &msg.size, sizeof(msg.size));
            data += sizeof(msg.size);
            std::memcpy(data, msg.payload, msg.size);
          } else if constexpr (std::is_same_v<type, Time>) {
            write(std::span(buffer), msg);
          } else if constexpr (std::is_same_v<type, WireChunk>) {
            auto data = buffer.data();
            write(std::span(buffer), msg.timestamp);
            data += sizeof(msg.timestamp);
            std::memcpy(data, &msg.size, sizeof(msg.size));
            data += sizeof(msg.size);
            std::memcpy(data, msg.payload, msg.size);
          } else if constexpr (std::is_same_v<type, CodecHeader>) {
            auto data = buffer.data();
            std::memcpy(data, &msg.codecSize, sizeof(msg.codecSize));
            data += sizeof(msg.codecSize);
            std::memcpy(data, msg.codec, msg.codecSize);
            data += msg.codecSize;
            std::memcpy(data, &msg.size, sizeof(msg.size));
            data += sizeof(msg.size);
            std::memcpy(data, msg.payload, msg.size);
          } else if constexpr (std::is_same_v<type, Error>) {
            auto data = buffer.data();
            std::memcpy(data, &msg.errorCode, sizeof(msg.errorCode));
            data += sizeof(msg.errorCode);
            std::memcpy(data, &msg.errorSize, sizeof(msg.errorSize));
            data += sizeof(msg.errorSize);
            std::memcpy(data, msg.error, msg.errorSize);
            data += msg.errorSize;
            std::memcpy(data, &msg.errorMessageSize,
                        sizeof(msg.errorMessageSize));
            data += sizeof(msg.errorMessageSize);
            std::memcpy(data, msg.errorMessage, msg.errorMessageSize);
          } else {
            std::unreachable();
          }
        },
        message);
  }

}  // namespace brilliant::snapcast