#ifndef MACROS_HPP
#define MACROS_HPP
namespace SNJ {
#define FORCE_INLINE __attribute__((always_inline)) inline
#define NO_INLINE __attribute__((noinline))
#define CACHE_ALIGN(decl) decl __attribute__((aligned(64)))
}  // namespace SNJ
#endif
