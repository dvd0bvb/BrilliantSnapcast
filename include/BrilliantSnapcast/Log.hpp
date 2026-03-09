#pragma once

#include <boost/log/attributes.hpp>
#include <boost/log/attributes/attribute_set.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/trivial.hpp>
#include <print>
#include <string_view>

namespace brilliant::snapcast {

  constexpr std::string_view CHANNEL{"BrilliantSnapcast"};
  using ChannelType = std::string;

  inline auto logger() -> boost::log::sources::severity_channel_logger_mt<
      boost::log::trivial::severity_level>& {
    static boost::log::sources::severity_channel_logger_mt<
        boost::log::trivial::severity_level, ChannelType>
        log(boost::log::keywords::channel = "BrilliantSnapcast");
    return log;
  }

  inline void init(const boost::log::attribute_set& attributes = {}) {
    auto& log = logger();
    log.set_attributes(attributes);
  }

  template <class... Ts>
  inline void doLog(boost::log::trivial::severity_level level,
                    std::format_string<Ts...> fmt, Ts&&... ts) {
    auto& log = logger();
    if (auto record = log.open_record(boost::log::keywords::severity = level)) {
      std::print(
          boost::log::aux::make_record_pump(log, record).stream().stream(), fmt,
          std::forward<Ts>(ts)...);
    }
  }

#ifdef BRILLIANT_SNAPCAST_DISABLE_LOGGING
#define BS_LOG(severity, format, ...)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define BS_LOG(s, fmt, ...)                                          \
  /* NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while) */             \
  do {                                                               \
    ::brilliant::snapcast::doLog(s, fmt __VA_OPT__(, ) __VA_ARGS__); \
  } while (false)
#endif

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define BS_LOG_TRACE(format, ...) \
  BS_LOG(boost::log::trivial::severity_level::trace, format, __VA_ARGS__)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define BS_LOG_DEBUG(format, ...) \
  BS_LOG(boost::log::trivial::severity_level::debug, format, __VA_ARGS__)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define BS_LOG_INFO(format, ...) \
  BS_LOG(boost::log::trivial::severity_level::info, format, __VA_ARGS__)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define BS_LOG_WARNING(format, ...) \
  BS_LOG(boost::log::trivial::severity_level::warning, format, __VA_ARGS__)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define BS_LOG_ERROR(format, ...) \
  BS_LOG(boost::log::trivial::severity_level::error, format, __VA_ARGS__)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define BS_LOG_FATAL(format, ...) \
  BS_LOG(boost::log::trivial::severity_level::fatal, format, __VA_ARGS__)

}  // namespace brilliant::snapcast