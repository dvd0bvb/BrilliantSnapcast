#pragma once

#include <BrilliantSnapcast/Message.hpp>
#include <BrilliantSnapcast/SnapConnection.hpp>
#include <BrilliantSnapcast/TimeProvider.hpp>
#include <BrilliantSnapcast/decoder/AnyDecoder.hpp>
#include <boost/asio/awaitable.hpp>
#include <system_error>
#include <type_traits>
#ifdef BRILLIANT_SNAPCAST_FLAC_DECODER
#include <BrilliantSnapcast/decoder/flac/FlacDecoder.hpp>
#endif
#include <BrilliantSnapcast/DecoderFilter.hpp>
#include <BrilliantSnapcast/Log.hpp>
#include <BrilliantSnapcast/ServerSettingsFilter.hpp>
#include <BrilliantSnapcast/decoder/pcm/PcmDecoder.hpp>
#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <chrono>
#include <memory_resource>
#include <string_view>
#include <vector>

namespace brilliant::snapcast {

  template <class Socket, class Pipeline>
  class SnapClient {
  public:
    SnapClient(Socket socket, Pipeline& inPipeline, TimeProvider& timeProvider,
               std::pmr::memory_resource* mr = std::pmr::get_default_resource())
        : _tcpConnection(std::move(socket), mr),
          _snapConnection(_tcpConnection),
          _pcmSink(inPipeline),
          _timeProvider(timeProvider),
          _decoder(decoder::pcm::PcmDecoder{}),
          _state(0) {
      setDecoder(inPipeline, _decoder);
    }

    void stop() { _tcpConnection.disconnect(); }

    auto start(TcpConnection<Socket>::protocol::endpoint ep,
               std::string_view mac, std::string_view host, std::string_view os,
               std::string_view arch, std::int32_t instance)
        -> boost::asio::awaitable<std::error_code> {
      if (auto ec = co_await _tcpConnection.connect(ep)) {
        BS_LOG_ERROR("Failed to connect: {} {}", ec.value(), ec.message());
        _tcpConnection.disconnect();  // just in case
        co_return ec;
      }

      std::pmr::vector<std::byte> buffer(
          4096, _tcpConnection.getAllocator().resource());
      if (auto ec = co_await _snapConnection.sendHello(
              mac, host, os, arch, instance, std::span(buffer))) {
        BS_LOG_ERROR("Failed to send hello to server: {} {}", ec.value(),
                     ec.message());
        _tcpConnection.disconnect();
        co_return ec;
      }      
      co_return std::error_code{};
    }

    auto handleIncoming() -> boost::asio::awaitable<std::error_code> {
      std::pmr::vector<std::byte> buffer(4096, _tcpConnection.getAllocator().resource());

      while (_tcpConnection.isConnected()) {
        auto result = co_await _snapConnection.read(std::span(buffer));
        if (result) {
          const auto& [base, msg] = *result;
          switch (base.type) {
          case MessageType::CODEC_HEADER: {
            const auto& header = std::get<CodecHeader>(msg);
            std::pmr::string codec(header.codec, header.codecSize,
                                   _tcpConnection.getAllocator().resource());
            BS_LOG_INFO("Codec is {}", codec);
            if (codec == "pcm") {
              _decoder.resetDecoder<decoder::pcm::PcmDecoder>();
              BS_LOG_INFO("Set up pcm decoder");
            }
#ifdef BRILLIANT_SNAPCAST_FLAC_DECODER
            else if (codec == "flac") {
              _decoder.resetDecoder<decoder::flac::FlacDecoder>();
              BS_LOG_INFO("Set up flac decoder");
            }
#endif
            else {
              BS_LOG_ERROR("Failed to set up a decoder");
              _tcpConnection.disconnect();
            }

            if (auto formatResult = _decoder.getFormat(
                    std::span(header.payload, header.size))) {
              const auto& format = *formatResult;
              _pcmSink.setFormat(format);
              _state |= StateFlag::GOT_CODEC_HEADER;
              BS_LOG_INFO("Set format {}:{}:{}", format.sampleRate, format.numChannels, format.bitDepth * 8);
            } else {
              const auto& ec = formatResult.error();
              BS_LOG_ERROR("There was an error parsing the format: {} {}",
                           ec.value(), ec.message());
              _tcpConnection.disconnect();
              co_return ec;
            }
          } break;
          case MessageType::SERVER_SETTINGS: {
            const auto& settings = std::get<ServerSettings>(msg);
            std::string_view json(settings.payload, settings.size);
            BS_LOG_DEBUG("Received server settings: {}", json);

            bool settingsOk = true;
            if (const auto e2eIdx = json.find(R"("bufferMs":)");
                e2eIdx != std::string_view::npos) {
              const auto start = e2eIdx + sizeof(R"("bufferMs":)") - 1;
              const auto end = json.substr(start).find_first_of(",}");
              std::int64_t e2e{};
              if (auto fcResult = std::from_chars(json.data() + start,
                                                  json.data() + end, e2e);
                  fcResult.ec == std::errc{}) {
                setEndToEndLatency(_pcmSink, std::chrono::milliseconds{e2e});
              } else {
                BS_LOG_ERROR("Failed to parse bufferMs in json string");
                _tcpConnection.disconnect();
                settingsOk = false;
                co_return std::make_error_code(fcResult.ec);
              }
            } else {
              settingsOk = false;
            }
            if (const auto volIdx = json.find(R"("volume":)");
                volIdx != std::string_view::npos) {
              const auto start = volIdx + sizeof(R"("volume":)") - 1;
              const auto end = json.substr(start).find_first_of(",}");
              std::uint32_t volume{};
              if (auto fcResult = std::from_chars(json.data() + start,
                                                  json.data() + end, volume);
                  fcResult.ec == std::errc{}) {
                brilliant::snapcast::setVolume(_pcmSink, volume);
              } else {
                BS_LOG_ERROR("Failed to parse volume in json string");
                _tcpConnection.disconnect();
                settingsOk = false;
                co_return std::make_error_code(fcResult.ec);
              }
            } else {
              settingsOk = false;
            }
            if (settingsOk) {
              _state |= StateFlag::GOT_SERVER_SETTINGS;
            }
          } break;
          case MessageType::TIME:
            _timeProvider.addTime(base, std::get<Time>(msg));
            break;
          case MessageType::WIRE_CHUNK:
            if (_state == READY) {
              const auto& chunk = std::get<WireChunk>(msg);
              _pcmSink.write(std::chrono::microseconds(chunk.timestamp),
                             std::span(chunk.payload, chunk.size));
            } else {
              BS_LOG_DEBUG("State is not ready {:#x}", _state);
            }
            break;
          case MessageType::ERROR: {
            const auto& error = std::get<Error>(msg);
            BS_LOG_WARNING(
                "Received an error from the server: {} {} {}", error.errorCode,
                std::string_view{error.error, error.errorSize},
                std::string_view{error.errorMessage, error.errorMessageSize});
          } break;
          }
        } else {
          auto& ec = result.error();
          BS_LOG_ERROR("Error reading message from server: {} {}", ec.value(),
                       ec.message());
          _tcpConnection.disconnect();
          co_return ec;
        }
      }
    }

    auto sendTime() -> boost::asio::awaitable<std::error_code> {
      using namespace std::chrono_literals;

      std::array<std::byte, sizeof(Base) + sizeof(Time)> buffer{};

      auto exec = co_await boost::asio::this_coro::executor;

      boost::asio::steady_timer timer(exec);
      auto useAwaitable = boost::asio::bind_allocator(
          _tcpConnection.getAllocator(), boost::asio::use_awaitable);
      auto warmup = 60;
      while (_tcpConnection.isConnected()) {
        if (auto ec =
                co_await _snapConnection.send(0, Time{}, std::span(buffer))) {
          BS_LOG_ERROR("Error sending client time: {} {}", ec.value(),
                       ec.message());
          _tcpConnection.disconnect();
          co_return ec;
        }
        if (warmup > 0) {
          --warmup;
          if (warmup == 0) {
            BS_LOG_DEBUG("Synced time");
            _state |= StateFlag::TIME_SYNCED;
          }
        }
        const auto timeout = warmup > 0 ? 1ms : 1000ms;
        timer.expires_from_now(timeout);
        if (auto [ec] =
                co_await timer.async_wait(boost::asio::as_tuple(useAwaitable));
            ec) {
          BS_LOG_WARNING("Error waiting on Time timer: {} {}", ec.value(),
                         ec.message());
        }
      }
    }

    void setVolume(std::uint32_t volume, bool muted) {
      // support setting volume from outside
      // send client info
      setVolume(_pcmSink, volume);
    }

  private:
    TcpConnection<Socket> _tcpConnection;
    SnapConnection<Socket> _snapConnection;
    Pipeline& _pcmSink;
    TimeProvider& _timeProvider;
    decoder::AnyDecoder _decoder;

    enum StateFlag : std::uint8_t {
      GOT_CODEC_HEADER = 0x1 << 0,
      GOT_SERVER_SETTINGS = 0x1 << 1,
      TIME_SYNCED = 0x1 << 2
    };

    static constexpr auto READY =
        GOT_CODEC_HEADER | GOT_SERVER_SETTINGS | TIME_SYNCED;

    std::underlying_type_t<StateFlag> _state;
  };

}  // namespace brilliant::snapcast
