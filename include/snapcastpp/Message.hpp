#pragma once

#include "messagetype.hpp"
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace snapcastpp {

#pragma pack(1)

struct Time {
  std::uint32_t sec;
  std::uint32_t usec;
};

struct Base {
  MessageType type;
  std::uint16_t id;
  std::uint16_t refersTo;
  Time sent;
  Time received;
  std::uint32_t size;
};

#pragma pack()

struct JsonMessage : Base {
  JsonMessage(MessageType type, std::uint16_t id, std::string payload)
      : Base{.type = type,
             .id = id,
             .refersTo = 0,
             .sent = {},
             .received = {},
             .size = static_cast<uint32_t>(
                 sizeof(Base) + sizeof(std::uint32_t) + payload.size())},
        size(static_cast<uint32_t>(payload.size())),
        payload(std::move(payload)) {}

  std::uint32_t size;
  std::string payload;
};

struct Hello : JsonMessage {
  Hello(std::uint16_t id, std::string payload)
      : JsonMessage(MessageType::HELLO, id, std::move(payload)) {}
};

struct ServerSettings : JsonMessage {
  ServerSettings(std::uint16_t id, std::string payload)
      : JsonMessage(MessageType::SERVER_SETTINGS, id, std::move(payload)) {}
};

struct ClientInfo : JsonMessage {
  ClientInfo(std::uint16_t id, std::string payload)
      : JsonMessage(MessageType::CLIENT_INFO, id, std::move(payload)) {}
};

struct TimeMessage : Base {
  TimeMessage(std::uint16_t id)
      : Base{.type = MessageType::TIME,
             .id = id,
             .refersTo = 0,
             .sent = {},
             .received = {},
             .size = static_cast<uint32_t>(sizeof(Base) + sizeof(Time))},
        latency{} {}

  Time latency;
};

struct WireChunk : Base {
  WireChunk(std::uint16_t id, std::vector<std::byte> payload)
      : Base{.type = MessageType::WIRE_CHUNK,
             .id = id,
             .refersTo = 0,
             .sent = {},
             .received = {},
             .size =
                 static_cast<uint32_t>(sizeof(Base) + sizeof(Time) +
                                       sizeof(std::uint32_t) + payload.size())},
        timestamp{}, size(static_cast<std::uint32_t>(payload.size())),
        payload(std::move(payload)) {}

  Time timestamp;
  uint32_t size;
  std::vector<std::byte> payload;
};

using Message = std::variant<Hello, ServerSettings, ClientInfo, TimeMessage, WireChunk>;

} // namespace snapcastpp