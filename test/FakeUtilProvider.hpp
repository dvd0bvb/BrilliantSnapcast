#pragma once

#include "BrilliantSnapcast/UtilProvider.hpp"

struct FakeUtilProvider : public brilliant::snapcast::UtilProvider {
  auto getMacAddress(int, std::pmr::memory_resource* mr)
      -> std::pmr::string override {
    return {"01:02:03:04:05:06:07:08:09:0a:0b:0c", mr};
  }

  auto getArch(std::pmr::memory_resource* mr) -> std::pmr::string override {
    return {"x86_64", mr};
  }

  auto getOS(std::pmr::memory_resource* mr) -> std::pmr::string override {
    return {"Ubuntu", mr};
  }
};
