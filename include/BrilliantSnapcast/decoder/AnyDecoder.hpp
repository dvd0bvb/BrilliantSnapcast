#pragma once

#include <chrono>
#include <type_traits>
#include <variant>
// TODO can we register decoder types in their headers?
// avoids need to manually edit DecoderTypes
#include <BrilliantSnapcast/decoder/pcm/PcmDecoder.hpp>
#ifdef BRILLIANT_SNAPCAST_FLAC_DECODER
#include <BrilliantSnapcast/decoder/flac/FlacDecoder.hpp>
#endif

namespace brilliant::snapcast::decoder {

  using DecoderTypes = std::variant<pcm::PcmDecoder
#ifdef BRILLIANT_SNAPCAST_FLAC_DECODER
                                    ,
                                    flac::FlacDecoder
#endif
                                    >;

  class AnyDecoder {
  public:
    AnyDecoder() = default;

    template <class D>
    AnyDecoder(D decoder) {
      resetDecoder(std::move(decoder));
    }

    template <class D>
    void resetDecoder() {
      _decoder.emplace<D>();
    }

    template <class D>
    void resetDecoder(D decoder) {
      _decoder.emplace<std::remove_cvref_t<D>>(std::move(decoder));
    }

    template <class Source>
    auto decode(Source& source, std::chrono::microseconds dacLatency,
                std::span<std::byte> outBuffer) -> std::uint32_t {
      return std::visit(
          [&source, dacLatency, outBuffer](auto&& decoder) {
            return decoder.decode(source, dacLatency, outBuffer);
          },
          _decoder);
    }

    template <class Source, class Sink>
    auto decode(Source& source, Sink& sink) {
      return std::visit(
          [&source, &sink](auto&& decoder) {
            return decoder.decode(source, sink);
          },
          _decoder);
    }

    auto getFormat(std::span<const std::byte> input) {
      return std::visit(
          [input](auto&& decoder) { return decoder.getFormat(input); },
          _decoder);
    }

  private:
    DecoderTypes _decoder;
  };

}  // namespace brilliant::snapcast::decoder
