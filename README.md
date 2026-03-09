# BrilliantSnapcast 
[![Multiplatform Tests](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/cmake-multi-platform.yml) [![Asan](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/asan.yml/badge.svg)](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/asan.yml) [![Msan](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/msan.yml/badge.svg)](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/asan.yml) [![Tsan](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/tsan.yml/badge.svg)](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/tsan.yml) 
[![clang-format Check](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/clang-format-check.yml/badge.svg)](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/clang-format-check.yml) [![clang-tidy Check](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/clang-tidy-check.yml/badge.svg)](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/clang-tidy-check.yml) [![cmake-format Check](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/cmake-format-check.yml/badge.svg)](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/cmake-format-check.yml)
![cppcheck](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2Fdvd0bvb%2FBrilliantSnapcast%2Fmaster%2F.github%2Fbadges%2Fcppcheck.json) ![coverage](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2Fdvd0bvb%2FBrilliantSnapcast%2Fmaster%2F.github%2Fbadges%2Fcoverage.json) ![Doxygen](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2Fdvd0bvb%2FBrilliantSnapcast%2Fmaster%2F.github%2Fbadges%2Fdoxygen.json) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT) <a href="https://www.buymeacoffee.com/dvd0bvb" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" style="height:20px !important;width: 80px !important;" ></a>

## A Snapcast Client Library

BrilliantSnapcast is a header only library aimed at providing utilities for creating a snapcast client targeting embedded devices. The goal of this project is to provide an intuitive interface for communicating with a snapcast server utilizing modern C++, to eliminate the need for exceptions and RTTI, and to add customization points for dynamic memory allocations and snapclient functionality.

All functionality has been tested and integrated with a client built and run on Ubuntu 24.04 (the code for this client is coming soon). Integration was performed with a snapserver running on the same Ubuntu machine (snapserver provided by the Ubuntu package manager) as well as with a remote snapserver provided by the excellent Music Assistant.

## Features
### Modern C++

BrilliantSnapcast implements network calls as C++ coroutines, using Boost.Asio under the hood. This minimizes the need to start multiple threads on platforms with limited threading resources or capabilities. Snapcast messages are returned to callers as a variant containing one of the different kinds of snapcast messages.

### Full Control

This library allows users to fully control dynamic memory allocations. A `std::pmr::memory_resource` is required to be provided to classes which utilize dynamic memory allocation. This mainly applies to allocations done by the Boost.Asio library for async handlers. For convenience, the memory_resource provided to a TcpClient instance can be utilized by SnapClient if no other memory_resource is provided to the SnapClient constructor.

Network calls utilize a user provided buffer, passed to read and write calls as a `std::span<std::byte>` instance. On a read operation, the provided buffers hold data read from the socket. Several of the Message types contain views into the buffer to avoid making additional copies of data. BrilliantSnapcast will detect if the buffer span is not long enough to store data for a read or write operation and return an appropriate error_code. 

To support embedded environments, no exceptions are thrown from any functions provided by BrilliantSnapcast. Results of calls are either a `std::error_code` or a `std::expected<ResultType, std::error_code>`.

BrilliantSnapcast does not provide name resolution at this time as `boost::asio::ip::tcp::resolver` stores IP address results as `std::string`s with no way to control allocation. If name resolution is desired, resolution and connection can be performed before passing the socket to a TcpClient instance.

The library provides the convenience class `SnapClient` which handles much of the boilerplate. See the example for usage. 

### Customization

BrilliantSnapcast provides the Pipeline class template which allows users to customize audio data processing pipelines by adding filters in front of or behind an `AudioDataDevice`. This library provides several common ones including a FLAC decoder and a `RateCorrectionFilter` which performs playback synchronization.

The `AudioDataDevice` class can use any container that conforms to the AudioDataDescriptorQueue and AudioDataByteQueue concepts. See AudioDataDevice.hpp for definitions of these concepts or CircularBufferAdapter in the tests for an example implementation.

Boost.Log is used for logging functionality. BrilliantSnapcast uses the channel name "BrilliantSnapcast" and `boost::log::trivial::severity_level` so logs can be filtered. 

The following preprocessor directives are used for build time customization:
|Directive|Description|
|---|---|
|BRILLIANT_SNAPCAST_DISABLE_LOGGING|Disable all logging from the build|
|BRILLIANT_SNAPCAST_FLAC_DECODER|Enable the use of the FLAC decoder. Requires linking to libflac++.|

## Example

A minimal example of a snapcast client program is provided below. This example assumes audio data will be consumed in a thread separate from the main thread. 

```c++
SomeThreadSafeQueue<brilliant::snapcast::AudioDataDescriptor> descriptorQueue(300);
SomeThreadSafeQueue<std::byte> audioDataQueue(300000);
brilliant::snapcast::AudioDataDevice device(descriptorQueue, audioDataQueue);

brilliant::snapcast::SharedServerSettings serverSettings;
brilliant::snapcast::ServerSettingsFilter serverSettingsFilter(serverSettings);
brilliant::snapcast::Pipeline in(serverSettingsFilter, device);

boost::asio::io_context context(1);

brilliant::snapcast::TimeProvider tp(.01, 0.0, 1.001);
boost::asio::ip::tcp::socket socket(context);
brillliant::snapcast::SnapClient client(std::move(socket), in, tp);

boost::asio::ip::tcp::resolver resolver(context);
resolver.async_resolve("localhost", "1704", [&in, &context, &client](boost::system::error_code ec, auto&& results){
    if (!ec) {
        boost::asio::co_spawn(context, [&client, &context, results = std::move(results)] -> boost::asio::awaitable<void> {
            // Assumes the user has a way to get MAC address, OS name, architecture strings
            auto ec = co_await client.start(context, *std::begin(results), ::getMacAddressString(), "BrilliantSnapClient", ::getOsString(), ::getArchString(), 1);
            if (ec) {
                // do error handling
                client.stop();
            }

            boost::asio::co_spawn(context, [&client] -> boost::asio::awaitable<void> {
                auto ec = co_await client.sendTime();
                if (ec) {
                    // do error handling
                    client.stop();
                }
            }, boost::asio::detached);

            ec = co_await client.handleIncoming();
            if (ec) {
                // do error handling
                client.stop();
            }
        }, boost::asio::detached);
    }
});

// Assumes an audio player class exists that starts a thread which reads from the output pipeline
brilliant::snapcast::RateCorrectionFilter rateCorrection(tp, serverSettings);
brilliant::snapcast::Pipeline out(rateCorrection, device);
AudioPlayer player(out);
player.start();

context.run();
```

## Next Steps

- Complete documentation
- Extend test coverage
- Support for big endian systems. Currently only supports little endian.
- Support the clang github workflows. Unfortunately, libstdc++ (clang's default standard library implementation) does not support `std::expected` which causes workflows using clang to fail. However, libc++ does support it so the cmake needs to be modified to build the tests with libc++ and gtest must be built with clang/libc++ as well. This combination builds and the tests pass as can be seen by the MSAN workflow. This would fix the ASAN and TSAN workflows at the same time.
- Support the MSVC workflow.

## Supporting the project

If you have suggestions please feel free to open an issue or create a PR.

You can support me directly via Buy Me a Coffee [here](https://www.buymeacoffee.com/dvd0bvb).

This project was seeded using [BrilliantCMake](https://www.github.com/dvd0bvb/BrilliantCMake).
