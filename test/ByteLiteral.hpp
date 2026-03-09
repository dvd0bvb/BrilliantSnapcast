#pragma once

#include <cstddef>

namespace byte_literal {
  constexpr auto operator""_b(unsigned long long int c) -> std::byte {
    return static_cast<std::byte>(c);
  }
}  // namespace byte_literal