#pragma once

#include <memory_resource>
#include <boost/container/pmr/memory_resource.hpp>

namespace brilliant::snapcast {
  /**
   * @brief Wraps a std::pmr::memory_resource to make it compatible with
   * boost::container::pmr::memory_resource
   *
   */
  class BoostPmrWrapper : public boost::container::pmr::memory_resource {
  public:
    /**
     * @brief Construct a new Boost Pmr Wrapper object
     * 
     * @param mr A pointer to the memory resource
     */
    BoostPmrWrapper(std::pmr::memory_resource* mr) : _mr(mr) {}

  private:
    /**
     * @brief Allocate implementation
     * 
     * @param bytes Number of bytes to allocate
     * @param alignment Allocation alignment
     * @return A pointer to allocated memory
     */
    [[nodiscard]] auto do_allocate(std::size_t bytes,
                                   std::size_t alignment) -> void* override {
      return _mr->allocate(bytes, alignment);
    }

    /**
     * @brief Deallocate implementation
     * 
     * @param p Pointer to the memory to deallocate
     * @param bytes The number of bytes in the allocation
     * @param alignment The allocation alignment
     */
    void do_deallocate(void* p, std::size_t bytes,
                       std::size_t alignment) override {
      _mr->deallocate(p, bytes, alignment);
    }

    /**
     * @brief Equality check implementation
     * 
     * @param mr The memory resource to compare to
     * @return True if the memory resources are equal
     */
    [[nodiscard]] auto do_is_equal(const boost::container::pmr::memory_resource&
                                       mr) const noexcept -> bool override {
      // TODO(david): dynamic_cast requires RTTI which is not always enabled in embedded environments
      if (const auto wrapper = dynamic_cast<const BoostPmrWrapper*>(&mr);
          wrapper && wrapper->_mr) {
        return _mr->is_equal(*wrapper->_mr);
      }
      return false;
    }

    /// Pointer to the underlying memory_resource
    std::pmr::memory_resource* _mr;
  };
}  // namespace brilliant::snapcast