#ifndef SPSC_QUEUE_HPP
#define SPSC_QUEUE_HPP
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "Macros.hpp"

namespace SNJ {
  template <class T, std::size_t QueueSize = 1024>
  class SPSCQueue {
   public:
    template <class... Args>
    FORCE_INLINE void Enqueue(Args&&... __args) {
      std::size_t writeIndex     = _mTail.load(std::memory_order_relaxed);
      std::size_t nextWriteIndex = getNextIndex(writeIndex);
      while (nextWriteIndex == _mHead.load(std::memory_order_relaxed));
      new (&_mDataBuffer[writeIndex]) T(std::forward<Args>(__args)...);
      _mTail.store(nextWriteIndex, std::memory_order_release);
    }

    FORCE_INLINE void Enqueue(T& __data) {
      std::size_t writeIndex     = _mTail.load(std::memory_order_relaxed);
      std::size_t nextWriteIndex = getNextIndex(writeIndex);
      while (nextWriteIndex == _mHead.load(std::memory_order_relaxed));
      _mDataBuffer[writeIndex] = __data;
      _mTail.store(nextWriteIndex, std::memory_order_release);
    }

    FORCE_INLINE bool Dequeue(T& __data) {
      if (IsEmpty()) return false;
      std::size_t readIndex = _mHead.load(std::memory_order_relaxed);
      memcpy(&__data, &_mDataBuffer[readIndex], sizeof(T));
      _mHead.store(getNextIndex(readIndex), std::memory_order_relaxed);
      return true;
    }

    FORCE_INLINE bool IsEmpty() {
      if (_mHead == _mTail.load(std::memory_order_acquire)) return true;
      return false;
    }

   private:
    std::size_t getNextIndex(std::size_t __index) { return (__index + 1) & kIndexMask; }

    inline static constexpr std::size_t kIndexMask = QueueSize - 1;
    T                                   _mDataBuffer[QueueSize];
    CACHE_ALIGN(std::atomic<std::size_t>) _mHead{0};
    CACHE_ALIGN(std::atomic<std::size_t>) _mTail{0};
  };
}  // namespace SNJ
#endif
