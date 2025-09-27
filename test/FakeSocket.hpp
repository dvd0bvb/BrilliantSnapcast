#pragma once

#include <algorithm>
#include <boost/asio.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/basic_socket.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/system/detail/error_code.hpp>

struct SocketState {
  std::vector<std::byte> inData;
  std::vector<std::byte> outData;
  boost::system::error_code ec;
  bool isConnected{false};
};

template <class Protocol, class Executor = boost::asio::any_io_executor>
class FakeSocket : public boost::asio::basic_socket<Protocol, Executor> {
  struct ConnectImpl {
    ConnectImpl(FakeSocket* s) : self(s) {}

    template <class Handler>
    void operator()(Handler h) {
      boost::asio::dispatch(
          self->get_executor(),
          std::bind(std::move(h), self->state->ec));
    }

    FakeSocket* self;
  };

  struct ReadOpImpl {
    ReadOpImpl(FakeSocket* s) : self(s) {}

    template <class Handler, class Buffer>
    void operator()(Handler h, const Buffer& buffer) {
      const auto bufSize = boost::asio::buffer_size(buffer);
      boost::asio::buffer_copy(
          buffer,
          boost::asio::buffer(std::span(self->state->inData).first(bufSize)));
      std::ranges::rotate(self->state->inData,
                  self->state->inData.begin() + static_cast<std::int32_t>(bufSize));
      boost::asio::dispatch(self->get_executor(),
                            std::bind(std::move(h), self->state->ec, bufSize));
    }

    FakeSocket* self;
  };

  struct WriteOpImpl {
    WriteOpImpl(FakeSocket* s) : self(s) {}

    template <class Handler, class Buffer>
    void operator()(Handler h, const Buffer& buffer) {
      const auto bufSize = boost::asio::buffer_size(buffer);
      self->state->outData.resize(bufSize);
      boost::asio::buffer_copy(boost::asio::buffer(self->state->outData),
                               buffer);
      boost::asio::dispatch(self->get_executor(),
                            std::bind(std::move(h), self->state->ec, bufSize));
    }

    FakeSocket* self;
  };

public:
  FakeSocket(Executor exec, SocketState* s)
      : boost::asio::basic_socket<Protocol, Executor>(exec), state(s) {}

  template <class Endpoint, class Handler>
  auto async_connect(const Endpoint&, Handler h) {
    //
    return boost::asio::async_initiate<Handler,
                                       void(boost::system::error_code)>(
        ConnectImpl(this), h);
  }

  template <class Buffers, class Handler>
  auto async_read_some(const Buffers& buffers, Handler h) {
    return boost::asio::async_initiate<Handler, void(boost::system::error_code,
                                                     std::size_t)>(
        ReadOpImpl(this), h, buffers);
  }

  template <class Buffers, class Handler>
  auto async_write_some(const Buffers& buffers, Handler h) {
    return boost::asio::async_initiate<Handler, void(boost::system::error_code,
                                                     std::size_t)>(
        WriteOpImpl(this), h, buffers);
  }

  SocketState* state;
};