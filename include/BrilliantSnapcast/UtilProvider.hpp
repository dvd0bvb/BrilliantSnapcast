#pragma once

#include <memory_resource>
#include <string>

namespace brilliant::snapcast {

  /**
   * @brief Provides platform depenedent information used for Hello message
   * fields such as MAC address, OS string, and platform architecture.
   *
   */
  class UtilProvider {
  public:
    /**
     * @brief Destroy the Util Provider object
     *
     */
    virtual ~UtilProvider() = default;

    /**
     * @brief Get the MAC Address as a string
     *
     * @param sock The socket handle
     * @param mr A pointer to the memory resource used to allocate the string
     * @return The MAC address as a string
     */
    virtual auto getMacAddress(int sock, std::pmr::memory_resource* mr)
        -> std::pmr::string = 0;

    /**
     * @brief Get the OS name
     *
     * @param mr The memory resource used to allocate the string
     * @return The OS string
     */
    virtual auto getOS(std::pmr::memory_resource* mr) -> std::pmr::string = 0;

    /**
     * @brief Get the platform architecture string, eg: x86_64
     *
     * @param mr The memory resource used to allocate the string
     * @return The platform architecture string1
     */
    virtual auto getArch(std::pmr::memory_resource* mr) -> std::pmr::string = 0;
  };

}  // namespace brilliant::snapcast