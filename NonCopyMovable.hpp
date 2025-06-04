#ifndef NONCOPYMOVABLE_HPP
#define NONCOPYMOVABLE_HPP

/**
 * @brief Macro to delete the copy constructor and copy assignment operator for a class,
 *        making the class non-copyable.
 *
 * Usage:
 * To use this macro, add `MAKE_NON_COPYABLE(ClassName)` inside the class definition.
 * This will prevent instances of the class from being copied.
 *
 * @param C The class name where copy operations should be disabled.
 */
#define MAKE_NON_COPYABLE(C)                \
  C(const C&) noexcept            = delete; \
  C& operator=(const C&) noexcept = delete;

/**
 * @brief Macro to delete the move constructor and move assignment operator for a class,
 *        making the class non-movable.
 *
 * Usage:
 * To use this macro, add `MAKE_NON_MOVABLE(ClassName)` inside the class definition.
 * This will prevent instances of the class from being moved.
 *
 * @param C The class name where move operations should be disabled.
 */
#define MAKE_NON_MOVABLE(C)            \
  C(C&&) noexcept            = delete; \
  C& operator=(C&&) noexcept = delete;

#endif  // NONCOPYMOVABLE_HPP