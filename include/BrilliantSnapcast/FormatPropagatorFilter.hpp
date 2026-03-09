#pragma once

#include <BrilliantSnapcast/Format.hpp>
#include <tuple>

namespace brilliant::snapcast {

  template <class... Ts>
  class FormatPropagatorFilter {
  public:
    FormatPropagatorFilter(Ts&... es) : _elements(es...) {}

    void setFormat(const Format& format) {
      std::apply([&format](auto&&... es) { (es.setFormat(format), ...); }, _elements);
    }

  private:
    std::tuple<Ts&...> _elements;
  };

}  // namespace brilliant::snapcast