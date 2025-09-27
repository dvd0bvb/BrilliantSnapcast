#pragma once

#include <cstdint>

namespace brilliant::snapcast {

  enum class MessageType : std::uint16_t {
    BASE = 0,
    CODEC_HEADER,
    WIRE_CHUNK,
    SERVER_SETTINGS,
    TIME,
    HELLO,
    CLIENT_INFO,
    ERROR
  };

}  // namespace brilliant::snapcast
