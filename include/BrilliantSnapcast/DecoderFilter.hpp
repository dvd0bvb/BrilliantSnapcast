#pragma once

#include <BrilliantSnapcast/decoder/AnyDecoder.hpp>
#include <type_traits>

namespace brilliant::snapcast {

  /**
   * @brief Decoder filter for audio data streams
   *
   * This filter applies a decoder to audio data chunks as they are written
   * through the filter. It uses an internal buffer for the decoder output.
   * If a decoder is not provided, the filter acts as a pass-through.
   *
   * DecoderFilter is templated on the decoder type to allow for different
   * decoding implementations without enforcing runtime polymorphism. This may
   * be useful not only for performance sensitive applications but also for
   * applications which know the decoder type at compile time. Applications
   * requiring runtime polymorphism can use a decoder interface as the template
   * parameter. BrilliantSnapcast provides the AnyDecoder class to support this.
   *
   * @tparam Decoder Type of the decoder to use
   */
  template <class Decoder>
  class DecoderFilter {
  public:
    /**
     * @brief Construct a new Decoder Filter object
     *
     * @param decoder Pointer to the decoder to use
     * @param workspaceSize Size of the decoder workspace in bytes
     * @param mr Pointer to the memory resource to use for allocations
     */
    DecoderFilter(Decoder& decoder) : _decoder(&decoder) {}

    /**
     * @brief Construct a new Decoder Filter object without a decoder
     *
     * @param workspaceSize Size of the decoder workspace in bytes
     * @param mr Pointer to the memory resource to use for allocations
     */
    DecoderFilter() : _decoder(nullptr) {}

    /**
     * @brief Set the decoder to use
     *
     * @param decoder Reference to the decoder
     */
    void setDecoder(Decoder& decoder) {
      _decoder = &decoder; 
    }

    template <class Source, std::size_t Extent>
    auto read(Source& source, std::chrono::microseconds dacLatency,
              std::span<std::byte, Extent> buffer) -> std::uint32_t {
      if (_decoder) {
        auto [bytesRead, ec] = _decoder->decode(source, dacLatency, buffer);
        // TODO do something with ec
        return bytesRead;
      } else {
        return source.read(dacLatency, buffer);
      }
    }

    template <class Source, class Sink>
    auto readTo(Source& source, Sink& sink) {
      if (_decoder) {
        auto [bytesRead, ec] = _decoder->decode(source, sink);
        // TODO(david): do something with ec
        return bytesRead;
      } else {
        return source.readTo(sink);
      }
    }

  private:
    /// Pointer to the decoder to use
    Decoder* _decoder;
  };

  template <class Pipeline, class Decoder>
  void setDecoder(Pipeline& pipeline, Decoder& decoder) {
    pipeline.visit([&decoder](DecoderFilter<std::remove_cvref_t<Decoder>>& filter) {
      filter.setDecoder(decoder);
    });
  }

}  // namespace brilliant::snapcast