# BrilliantSnapcast 
[![Multiplatform Tests](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/cmake-multi-platform.yml) [![Asan](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/asan.yml/badge.svg)](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/asan.yml) [![Msan](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/msan.yml/badge.svg)](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/asan.yml) [![Tsan](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/tsan.yml/badge.svg)](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/tsan.yml) 
[![clang-format Check](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/clang-format-check.yml/badge.svg)](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/clang-format-check.yml) [![clang-tidy Check](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/clang-tidy-check.yml/badge.svg)](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/clang-tidy-check.yml) [![cmake-format Check](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/cmake-format-check.yml/badge.svg)](https://github.com/dvd0bvb/BrilliantSnapcast/actions/workflows/cmake-format-check.yml)
[![coverage](img/coverage.svg)](https://dvd0bvb.github.io/BrilliantSnapcast) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT) <a href="https://www.buymeacoffee.com/dvd0bvb" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" style="height:20px !important;width: 80px !important;" ></a>

## A Snapcast Client Library

BrilliantSnapcast is a header only library aimed at providing utilities for creating a snapcast client targeting embedded devices. The goal of this project is to provide an intuitive interface for communicating with a snapcast server by utilizing modern C++ and providing mechanisms to control dynamic memory allocations. 

### Modern C++

BrilliantSnapcast implements network calls as C++ coroutines, using Boost.Asio under the hood. This eliminates the need to start multiple threads on platforms with limited resources. Snapcast messages are returned to callers as a variant containing one of the different kinds of snapcast messages.

### Full Control

This library allows users to fully control dynamic memory allocations. A `std::pmr::memory_resource` is required to be provided to classes which utilize dynamic memory allocation. This mainly applies to allocations done by the Boost.Asio library for async handlers and Boost.Json object construction for SnapClient::sendHello(). For convenience, the memory_resource provided to a TcpClient instance can be utilized by SnapClient if no other memory_resource is provided to the SnapClient constructor.

Network calls utilize a user provided buffer, passed to read and write calls as a `std::span<std::byte>` instance. On a read operation, the provided buffers hold data read from the socket. Several of the Message types contain views into the buffer to avoid making additional copies of the data. BrilliantSnapcast will detect if the buffer span is not long enough to store data for a read or write operation and return an appropriate error_code.

To support embedded environments, no exceptions are thrown from any functions provided by BrilliantSnapcast. Results of calls are either a `boost::system::error_code` or a `std::expected<ResultType, boost::system::error_code>`.

BrilliantSnapcast does not provide name resolution at this time as `boost::asio::ip::tcp::resolver` stores IP address results as `std::string`s with no way to control allocation. If name resolution is desired, resolution and connection can be performed before passing the socket to a TcpClient instance.

### Example

```c++
boost::asio::io_context context;
boost::asio::ip::tcp::socket socket(context);
brilliant::snapcast::TcpClient client(std::move(socket), std::pmr::get_default_resource());
brilliant::snapcast::SnapClient snapClient(client);
boost::json::serializer serializer;

boost::asio::co_spawn(context, [&context, &client, &snapClient, &serializer] -> boost::asio::awaitable<void> {
    std::array<std::byte, 4096> buffer;
        
    co_await client.connect("127.0.0.1", 1704);

    MyUtilProvider utilProvider; // MyUtilProvider implements the brilliant::snapcast::UtilProvider interface
    co_await snapClient.sendHello(utilProvider, serializer, std::span(buffer));

    // start a coroutine to periodically send Time messages
    boost::asio::co_spawn(context, [&client, &snapClient] -> boost::asio::awaitable<void> {
        using namespace std::chrono_literals;

        std::array<std::byte, sizeof(brilliant::snapcast::Base) + sizeof(brilliant::snapcast::Time)> buffer;
        auto exec = co_await boost::asio::this_coro::executor;
        boost::asio::steady_timer timer(exec);

        while (client.isConnected()) {
            // Time values are populated in the send() call
            if (auto result = co_await snapClient.send(0, brilliant::snapcast::Time{}, std::span(buffer)); result.has_value()) {
                const brilliant::snapcast::Time sentTime = result.value()
                storeLastTimeSent(sentTime);
                timer.expires_after(1s);
                co_await timer.async_wait(boost::asio::use_awaitable);
            } else {
                handleError(result.error());
            }            
        }
    }, boost::asio::detached);

    // main message handling loop
    while (client.isConnected()) {
        auto result = co_await snapClient.read(std::span(buffer));
        result.and_then(&handleMessage).or_else(&handleError);
    }
}, boost::asio::detached);

context.run();
```

## Next Steps

- Investigate abstractions for providing network latency based on sent and received Time messages.
- Investigate abstractions for providing audio data including codec information and WireChunks, possibly providing an audio decoding pipeline and a way to access decoded chunks of data.
- Support the clang github workflows. Unfortunately, libstdc++ (clang's default standard library implementation) does not support `std::expected` which causes workflows using clang to fail. However, libc++ does support it so the cmake needs to be modified to build the tests with libc++ and gtest must be built with clang/libc++ as well. This combination builds and the tests pass as can be seen by the MSAN workflow. This would fix the ASAN and TSAN workflows at the same time.
- Support the MSVC workflow.

## Supporting the project

If you have suggestions please feel free to open an issue or create a PR.

You can support me directly via Buy Me a Coffee [here](https://www.buymeacoffee.com/dvd0bvb).

This project was seeded using [BrilliantCMake](https://www.github.com/dvd0bvb/BrilliantCMake).
