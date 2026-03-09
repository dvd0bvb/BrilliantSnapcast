#pragma once

#include <BrilliantSnapcast/AudioDataDescriptor.hpp>
#include <BrilliantSnapcast/DurationConversion.hpp>
#include <BrilliantSnapcast/Format.hpp>
#include <BrilliantSnapcast/Log.hpp>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <span>

namespace brilliant::snapcast {

  template <class T>
  concept AudioQueue = requires(T& t) {
    { t.empty() } -> std::convertible_to<bool>;
    { t.write_available() } -> std::convertible_to<std::size_t>;
  };

  template <class T>
  concept AudioDescriptorQueue =
      requires(T& t) {
        typename T::value_type;
        { t.front() };
        { t.pop() } -> std::same_as<void>;
        {
          t.push(std::declval<typename T::value_type>())
        } -> std::same_as<void>;
      } && AudioQueue<T> &&
      std::same_as<typename T::value_type, AudioDataDescriptor>;

  template <class T>
  concept AudioByteQueue = requires(T& t, std::span<const std::byte> src,
                                    std::span<std::byte> dst, std::uint32_t n) {
    { t.push_range(src) } -> std::same_as<void>;
    { t.pop_range(dst) } -> std::same_as<void>;
    { t.pop(n) } -> std::same_as<void>;
  } && AudioQueue<T>;

  /**
   * @brief Device for reading and writing audio data chunks
   *
   * The class adheres to the boost::iostreams concept of a seekable device.
   * It maintains separate containers for audio data descriptors and audio data
   * bytes. The read and write operations handle audio data chunks based on the
   * provided descriptors.
   *
   * @tparam DescriptorContainer Container type for audio data descriptors
   * @tparam ByteContainer Container type for audio data bytes
   */
  template <AudioDescriptorQueue DescriptorContainer,
            AudioByteQueue ByteContainer>
  class AudioDataDevice {
  public:
    /// Deleted default constructor
    AudioDataDevice() = delete;

    /**
     * @brief Construct a new Audio Data Device object
     *
     * @param descriptors The container for audio data descriptors
     * @param bytes The container for audio data bytes
     */
    AudioDataDevice(DescriptorContainer& descriptors, ByteContainer& bytes)
        : _descriptors(&descriptors), _bytes(&bytes), _bytePos(0), _format{} {}

    void setFormat(const Format& format) noexcept { _format = format; }

    [[nodiscard]] auto getFormat() const noexcept -> const Format& {
      return _format;
    }

    /**
     * @brief Read audio data into a buffer
     *
     * @tparam Extent The span extent
     * @param buffer The buffer to read into
     * @return The number of bytes read
     */
    template <std::size_t Extent>
    auto read(std::chrono::microseconds, std::span<std::byte, Extent> buffer)
        -> std::uint32_t {
      using namespace std::chrono_literals;

      std::uint32_t numRead{};
      while (!_descriptors->empty() && numRead < buffer.size()) {
        auto read = readOne(buffer.subspan(numRead));
        numRead += read;
      }
      return numRead;
    }

    /**
     * @brief Write data to the underlying buffers
     *
     * The functions emplace(DescriptorContainer&, Ts&&...) and
     * emplaceRange(ByteContainer&, std::span<std::byte, Extent>) can be
     * overloaded to provide custom implementations for adding to the buffers
     *
     * @tparam Extent The span extent
     * @param timepoint The timepoint of the audio chunk
     * @param buffer The data buffer
     * @return The number of bytes written
     */
    template <std::size_t Extent>
    auto write(const std::chrono::microseconds& timepoint,
               std::span<const std::byte, Extent> buffer) -> std::uint32_t {
      const auto added = append(buffer);
      commitAppend(timepoint, added);
      return added;
    }

    auto append(std::span<const std::byte> buffer) -> std::uint32_t {
      syncForWrite(static_cast<std::uint32_t>(buffer.size()));
      const auto bytesCapacity = _bytes->write_available();
      const auto toAdd =
          static_cast<std::uint32_t>(std::min(bytesCapacity, buffer.size()));
      _bytes->push_range(buffer.first(toAdd));
      return toAdd;
    }

    void commitAppend(std::chrono::microseconds timepoint,
                      std::uint32_t chunkSize) {
      _descriptors->push(
          AudioDataDescriptor{.timepoint = timepoint, .chunkSize = chunkSize});
    }

    /**
     * @brief Seek to the next audio chunk
     *
     */
    void seekToNextChunk() {
      if (!_descriptors->empty()) {
        const auto chunkSize = _descriptors->front().chunkSize;
        _descriptors->pop();
        _bytes->pop(chunkSize - _bytePos);
        _bytePos = 0;
      }
    }

    /**
     * @brief Get the current descriptor's timepoint if there is one
     *
     * @return An optional containing the first descriptor's timepoint if the
     * descriptor buffer is not empty. A std::nullopt otherwise.
     */
    [[nodiscard]] auto peek() -> std::optional<AudioDataDescriptor> {
      if (!_descriptors->empty() && _format.sampleRate != 0) {
        const auto& desc = _descriptors->front();
        return AudioDataDescriptor{
            .timepoint =
                desc.timepoint +
                bytesToDuration<std::chrono::microseconds>(_bytePos, _format),
            .chunkSize = desc.chunkSize - _bytePos};
      }
      return std::nullopt;
    }

  private:
    /**
     * @brief Read one audio data chunk
     *
     * The function popRange(Container&, span) can be overloaded to provide a
     * read implementation for ByteContainer
     *
     * @param timepoint The read chunk's timepoint
     * @param buffer The buffer to read into
     * @return Number of bytes actually read
     */
    template <std::size_t Extent>
    auto readOne(std::span<std::byte, Extent> buffer) -> std::uint32_t {
      const auto& descriptor = _descriptors->front();
      const auto bytesRemainingInChunk = descriptor.chunkSize - _bytePos;
      const auto readSize = std::min(static_cast<std::uint32_t>(buffer.size()),
                                     bytesRemainingInChunk);

      _bytes->pop_range(buffer.first(readSize));
      if (readSize == bytesRemainingInChunk) {
        _descriptors->pop();
        _bytePos = 0;
      } else {
        _bytePos += readSize;
      }
      return readSize;
    }

    void syncForWrite(std::uint32_t bytesToWrite) {
      while (!_descriptors->empty() && _descriptors->write_available() < 1 &&
             !_bytes->empty() && _bytes->write_available() < bytesToWrite) {
        BS_LOG_DEBUG("Dumping chunk. Writing {} when {} available",
                     bytesToWrite, _bytes->write_available());
        seekToNextChunk();
      }
    }

    /// Container for audio data descriptors
    DescriptorContainer* _descriptors;

    /// Container for audio data bytes
    ByteContainer* _bytes;

    /// Current byte position within the audio data chunk
    std::uint32_t _bytePos;

    /// Format of the contained audio data
    Format _format;
  };

}  // namespace brilliant::snapcast