#pragma once

#include <BrilliantSnapcast/AudioDataDevice.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/circular_buffer/base.hpp>
#include <memory_resource>
#include <ranges>
#include <span>

template <class T>
class CircularBufferAdapter
    : boost::circular_buffer<T, std::pmr::polymorphic_allocator<T>> {
public:
  using value_type = T;
  using size_type = std::size_t;
  using base_type =
      boost::circular_buffer<T, std::pmr::polymorphic_allocator<T>>;

  CircularBufferAdapter(size_type capacity, std::pmr::memory_resource* mr =
                                             std::pmr::get_default_resource())
      : base_type(capacity, mr) {}

  using base_type::empty;
  using base_type::front;

  [[nodiscard]] auto write_available() const noexcept -> std::size_t {
    return base_type::reserve();
  }

  void push(const T& t) { base_type::push_back(t); }

  void pop() { base_type::pop_front(); }

  void pop(std::uint32_t n) {
    base_type::erase_begin(static_cast<size_type>(n));
  }

  template <std::ranges::range R>
  void push_range(R&& range) {
    base_type::insert(base_type::end(), std::ranges::begin(range),
                      std::ranges::end(range));
  }

  template <std::ranges::contiguous_range R>
  void pop_range(R&& range) {
    const auto n =
        std::min<std::size_t>(base_type::size(), std::ranges::size(range));
    std::ranges::copy_n(base_type::begin(), n, std::begin(range));
    base_type::erase_begin(n);
  }
};
