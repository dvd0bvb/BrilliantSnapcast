#pragma once

#include <system_error>

namespace brilliant::snapcast::decoder {

  enum class DecoderErrc { INVALID_BIT_DEPTH };

  class DecoderErrorCategory : public std::error_category {
  public:
    [[nodiscard]] inline auto name() const noexcept -> const char* override {
      return "BrilliantSnapcastDecoderError";
    }

    [[nodiscard]] inline auto message(int e) const -> std::string override {
      const auto errc = static_cast<DecoderErrc>(e);
      switch (errc) {
      case DecoderErrc::INVALID_BIT_DEPTH:
        return "Invalid bit depth";
      }
    }
  };

}  // namespace brilliant::snapcast::decoder

namespace std {

  inline auto make_error_code(brilliant::snapcast::decoder::DecoderErrc errc) -> error_code {
    static brilliant::snapcast::decoder::DecoderErrorCategory category;
    return { static_cast<int>(errc), category };
  }

  template <>
  struct is_error_code_enum<brilliant::snapcast::decoder::DecoderErrc>
      : std::true_type {};

}  // namespace std