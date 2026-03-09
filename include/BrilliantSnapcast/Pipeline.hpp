#pragma once

#include <BrilliantSnapcast/AudioDataDescriptor.hpp>
#include <BrilliantSnapcast/Format.hpp>
#include <BrilliantSnapcast/decoder/AnyDecoder.hpp>
#include <chrono>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

namespace brilliant::snapcast {

  namespace {
    template <class D, class P>
    concept HasRead = requires(D d, P p) {
      d.read(p, std::declval<std::chrono::microseconds>(),
             std::declval<std::span<std::byte>>());
    };

    template <class D, class P1, class P2>
    concept HasReadTo = requires(D d, P1 p1, P2 p2) { d.readTo(p1, p2); };

    template <class D, class P>
    concept HasWrite = requires(D d, P p) {
      d.write(p, std::declval<std::chrono::microseconds>(),
              std::declval<std::span<const std::byte>>());
    };

    template <class D>
    concept HasSetFormat =
        requires(D d) { d.setFormat(std::declval<Format>()); };

  }  // namespace

  template <class Device, class... Devices>
  class Pipeline : public Pipeline<Devices...> {
  public:
    using BaseType = Pipeline<Devices...>;

    Pipeline(Device& d, Devices&... ds)
        : Pipeline<Devices...>(ds...), _device(d) {}

    auto read(std::chrono::microseconds dacLatency, std::span<std::byte> buffer)
        -> std::uint32_t {
      if constexpr (HasRead<Device, BaseType>) {
        return _device.read(static_cast<BaseType&>(*this), dacLatency, buffer);
      } else {
        return BaseType::read(buffer);
      }
    }

    template <class... Ds>
    auto readTo(Pipeline<Ds...>& pipeline) {
      if constexpr (HasReadTo<Device, BaseType, Pipeline<Ds...>>) {
        return _device.readTo(static_cast<BaseType&>(*this), pipeline);
      } else {
        return BaseType::readTo(pipeline);
      }
    }

    auto write(std::chrono::microseconds timestamp,
               std::span<const std::byte> buffer) -> std::uint32_t {
      if constexpr (HasWrite<Device, BaseType>) {
        return _device.write(static_cast<BaseType&>(*this), timestamp, buffer);
      } else {
        return BaseType::write(timestamp, buffer);
      }
    }

    void setFormat(const Format& format) {
      if constexpr (HasSetFormat<Device>) {
        _device.setFormat(format);
      } else {
        return BaseType::setFormat(format);
      }
    }

    template <class F, class... Args>
    auto visit(F f, Args&&... args) {
      if constexpr (std::is_invocable_v<F, Device&, Args...>) {
        f(_device, std::forward<Args>(args)...);
      } else {
        BaseType::visit(f, std::forward<Args>(args)...);
      }
    }

  private:
    Device& _device;
  };

  template <class Device>
  class Pipeline<Device> {
  public:
    Pipeline(Device& d) : _device(d) {}

    auto read(std::chrono::microseconds dacLatency, std::span<std::byte> buffer)
        -> std::uint32_t {
      return _device.read(dacLatency, buffer);
    }

    template <class... Ds>
    auto readTo(Pipeline<Ds...>& pipeline) {
      // TODO
      return 0u;
    }

    auto write(std::chrono::microseconds timestamp,
               std::span<const std::byte> buffer) -> std::uint32_t {
      return _device.write(timestamp, buffer);
    }

    auto append(std::span<const std::byte> buffer) -> std::uint32_t {
      return _device.append(buffer);
    }

    void commitAppend(std::chrono::microseconds timestamp,
                      std::uint32_t chunkSize) {
      _device.commitAppend(timestamp, chunkSize);
    }

    [[nodiscard]] auto peek() const -> std::optional<AudioDataDescriptor> {
      return _device.peek();
    }

    void seekToNextChunk() { _device.seekToNextChunk(); }

    void setFormat(const Format& format) noexcept { _device.setFormat(format); }

    [[nodiscard]] auto getFormat() const noexcept -> const Format& {
      return _device.getFormat();
    }

    template <class F, class... Args>
    void visit(F f, Args&&... args) {
      if constexpr (std::is_invocable_v<F, Device, Args...>) {
        f(_device, std::forward<Args>(args)...);
      }
    }

  private:
    Device& _device;
  };

}  // namespace brilliant::snapcast