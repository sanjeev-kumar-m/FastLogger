#ifndef SINGLETON_HPP
#define SINGLETON_HPP

#include "NonCopyMovable.hpp"

namespace SNJ {
  /**
   * @class class Singleton template
   * @brief A generic implementation of the Singleton design pattern.
   */
  template <class T>
  class Singleton {
   protected:
    /**
     * @brief Protected constructor for the singleton pattern.
     *
     * The constructor is private to prevent direct instantiation.
     */
    Singleton() noexcept = default;

    /**
     * @brief Using macro for disabling object copy functionality
     */
    MAKE_NON_COPYABLE(Singleton);

   public:
    /**
     * @brief Default move constructor to allow move semantics
     */
    Singleton(Singleton&&) noexcept = default;

    /**
     * @brief Default move assignment operator to allow move semantics
     */
    Singleton& operator=(Singleton&&) noexcept = default;

    /**
     * @brief static method to get the singleton instance
     * @param args arguments to the constructor of T
     */
    template <class... Args>
    static T& getInstance(Args&&... args) {
      // Static local variable to ensure the object is created only once
      static T Instance(std::forward<Args>(args)...);
      return Instance;
    }
  };
}  // namespace SNJ

#endif
