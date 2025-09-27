#pragma once

#include <string>
namespace snapcastpp {

class UtilProvider {
public:
  virtual ~UtilProvider() = default;

  virtual auto getMacAddress(int sock) -> std::string = 0;

  virtual auto getOS() -> std::string = 0;

  virtual auto getArch() -> std::string = 0;
};

}  // namespace snapcastpp