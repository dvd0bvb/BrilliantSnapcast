#pragma once

#include <FLAC++/decoder.h>
#include <FLAC/stream_decoder.h>

#include <BrilliantSnapcast/DurationConversion.hpp>
#include <BrilliantSnapcast/Format.hpp>
#include <BrilliantSnapcast/Log.hpp>
#include <BrilliantSnapcast/decoder/DecoderError.hpp>
#include <BrilliantSnapcast/decoder/flac/FlacError.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <span>
#include <system_error>

namespace brilliant::snapcast::decoder::flac {

  class FlacDecoder : public FLAC::Decoder::Stream {
  public:

    template <class Source, class Sink>
    auto decode(Source& source, Sink& sink) {
      if (auto descriptor = source.peek()) {
        struct Session {
          Source* src;
          Sink* sink;
          std::uint32_t readLimit;
          std::uint32_t outputSize;
          FlacDecoder* decoder;
        };
        Session session{&source, &sink, descriptor->chunkSize, 0u, this};

        _cbCtx = &session;
        _readFn = &FlacDecoder::read<Session>;
        _writeFn = &FlacDecoder::write<Session>;

        while (session.readLimit > 0) {
          this->process_single();
        }

        _cbCtx = nullptr;
        _readFn = nullptr;
        _writeFn = nullptr;

        sink.commitAppend(descriptor->timepoint, session.outputSize);

        const auto ec = _ec;
        _ec.clear();
        // TODO errors should actually be handled by the callers
        if (ec) {
          BS_LOG_ERROR("Error in decode: {} {}", ec.value(), ec.message());
        }
        return std::make_tuple(session.outputSize, ec);
      }
      return std::make_tuple(0u, _ec);
    }

    template <std::size_t Extent>
    auto getFormat(std::span<const std::byte, Extent> input)
        -> std::expected<Format, std::error_code> {
      struct FormatSession {
        const std::span<const std::byte, Extent>* input;
        std::size_t readLimit;
        std::uint32_t bytesRead;
      };
      FormatSession fs{&input, input.size(), 0u};

      _cbCtx = &fs;
      _readFn = &FlacDecoder::readFormat<FormatSession>;
      _writeFn = nullptr;

      
      if (const auto initStatus = this->init(); initStatus != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        _ec = std::make_error_code(FlacInitErrc{.ec = initStatus});
      } else {
        this->process_until_end_of_metadata();
      }

      _cbCtx = nullptr;
      _readFn = nullptr;

      if (_ec) {
        const auto ec = _ec;
        _ec.clear();
        return std::unexpected(ec);
      }
      return _format;
    }

  private:
    auto read_callback(::FLAC__byte buffer[], std::size_t* bytes)
        -> FLAC__StreamDecoderReadStatus override {
      if (_readFn && _cbCtx) {
        return _readFn(_cbCtx, buffer, bytes);
      }
      return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }

    auto write_callback(const ::FLAC__Frame* frame,
                        const ::FLAC__int32* const buffer[])
        -> FLAC__StreamDecoderWriteStatus override {
      if (_writeFn && _cbCtx) {
         return _writeFn(_cbCtx, frame, buffer);
      }
      return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    void metadata_callback(const ::FLAC__StreamMetadata* metadata) override {
      if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        const auto& streamInfo = metadata->data.stream_info;
        _format.sampleRate = streamInfo.sample_rate;
        _format.numChannels = static_cast<NumChannelType>(streamInfo.channels);

        if (streamInfo.bits_per_sample != 8 &&
            streamInfo.bits_per_sample != 16 &&
            streamInfo.bits_per_sample != 24 &&
            streamInfo.bits_per_sample != 32) {
          _ec = std::make_error_code(DecoderErrc::INVALID_BIT_DEPTH);
        } else {
          _format.bitDepth =
              static_cast<BitDepth>(streamInfo.bits_per_sample / 8);
        }
      }
    }

    void error_callback(::FLAC__StreamDecoderErrorStatus status) override {
      _ec = std::make_error_code(FlacDecoderErrc{status});
    }

    template <class Session>
    static auto read(void* ctx, ::FLAC__byte buffer[], std::size_t* bytes)
        -> FLAC__StreamDecoderReadStatus {
      using namespace std::chrono_literals;

      auto* s = static_cast<Session*>(ctx);
      const auto toRead = std::min(*bytes, static_cast<std::size_t>(s->readLimit));
      const auto actualRead = s->src->read(
          0us, std::span(reinterpret_cast<std::byte*>(buffer), toRead));
      *bytes = actualRead;
      s->readLimit -= actualRead;
      return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    }

    template <class Session>
    static auto write(void* ctx, const ::FLAC__Frame* frame,
                      const ::FLAC__int32* const buffer[])
        -> FLAC__StreamDecoderWriteStatus {
      auto* session = static_cast<Session*>(ctx);
      FlacDecoder* self = session->decoder;
      const auto sampleSize = frame->header.bits_per_sample / 8;
      const auto numChannels = frame->header.channels;
      const auto numSamples = frame->header.blocksize;

      // TODO test if this is more performant than copying directly into sink
      // (probably is, especially in the thread safe cases)
      std::uint32_t copied = 0;
      auto dst = self->_interleaveBuffer.data();
      for (std::uint32_t i = 0; i < numSamples; ++i) {
        for (std::uint32_t ch = 0; ch < numChannels; ++ch) {
          const auto* sample =
              reinterpret_cast<const std::byte*>(&buffer[ch][i]);
          std::memcpy(dst, sample, sampleSize);
          dst += sampleSize;
          copied += sampleSize;
          if ((self->_interleaveBuffer.size() - copied) < sampleSize) {
            session->outputSize +=
                session->sink->append(std::span<const std::byte>(
                    self->_interleaveBuffer.data(), copied));
            copied = 0;
            dst = self->_interleaveBuffer.data();
          }
        }
      }
      // in case any data leftover in the interleave buffer
      if (copied > 0) {
        session->outputSize += session->sink->append(
            std::span<const std::byte>(self->_interleaveBuffer.data(), copied));
      }
      return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    }

    template <class FormatSession>
    static auto readFormat(void* ctx, ::FLAC__byte buffer[], std::size_t* bytes)
        -> FLAC__StreamDecoderReadStatus {
      auto* s = static_cast<FormatSession*>(ctx);
      const auto toRead = std::min(*bytes, s->readLimit);
      std::memcpy(buffer, s->input->subspan(s->bytesRead, toRead).data(),
                  toRead);
      s->bytesRead += toRead;
      s->readLimit -= toRead;
      return s->readLimit == 0 ? FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM
                               : FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    }

    void* _cbCtx = nullptr;
    FLAC__StreamDecoderReadStatus (*_readFn)(void* ctx, ::FLAC__byte buffer[],
                                             std::size_t* bytes) = nullptr;
    FLAC__StreamDecoderWriteStatus (*_writeFn)(
        void* ctx, const ::FLAC__Frame* frame,
        const ::FLAC__int32* const buffer[]) = nullptr;
    std::array<std::byte, 512> _interleaveBuffer;
    std::error_code _ec;
    Format _format;
  };

}  // namespace brilliant::snapcast::decoder::flac