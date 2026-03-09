#pragma once

#include <FLAC/stream_decoder.h>
#include <system_error>
#include <FLAC++/decoder.h>

namespace brilliant::snapcast::decoder::flac {
  class FlacDecoderErrorCategory : public std::error_category {
  public:
    [[nodiscard]] inline auto name() const noexcept -> const char* override {
      return "LibFLACError";
    }

    [[nodiscard]] inline auto message(int e) const -> std::string override {
      return FLAC__StreamDecoderErrorStatusString[e];
    }
  };

  struct FlacDecoderErrc {
    ::FLAC__StreamDecoderErrorStatus ec;
  };

  class FlacInitErrorCategory : public std::error_category {
  public:
    [[nodiscard]] inline auto name() const noexcept -> const char* override {
      return "LibFLACError";
    }

    [[nodiscard]] inline auto message(int e) const -> std::string override {
      return FLAC__StreamDecoderInitStatusString[e];
    }
  };

  struct FlacInitErrc {
    ::FLAC__StreamDecoderInitStatus ec;
  };

}  // namespace brilliant::snapcast::decoder::flac

namespace std {
  inline auto make_error_code(brilliant::snapcast::decoder::flac::FlacDecoderErrc errc) -> std::error_code {
    static brilliant::snapcast::decoder::flac::FlacDecoderErrorCategory category;
    return { errc.ec, category };
  }

  template<> struct is_error_code_enum<brilliant::snapcast::decoder::flac::FlacDecoderErrc> : std::true_type {};

  inline auto make_error_code(brilliant::snapcast::decoder::flac::FlacInitErrc errc) -> std::error_code {
    static brilliant::snapcast::decoder::flac::FlacInitErrorCategory category;
    return { errc.ec, category };
  }

  template<> struct is_error_code_enum<brilliant::snapcast::decoder::flac::FlacInitErrc> : std::true_type {};
}