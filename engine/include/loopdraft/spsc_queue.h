// Lock-free single-producer / single-consumer ring buffer.
// One thread may call push(), one other thread may call pop(). No locks, no allocation after construction.
#pragma once
#include <atomic>
#include <cstddef>
#include <vector>

namespace ld {

template <typename T>
class SpscQueue {
 public:
  // capacity must be a power of two; one slot stays empty.
  explicit SpscQueue(size_t capacity) : buf_(capacity), mask_(capacity - 1) {}

  bool push(const T& v) {
    const size_t w = write_.load(std::memory_order_relaxed);
    const size_t next = (w + 1) & mask_;
    if (next == read_.load(std::memory_order_acquire)) return false;   // full
    buf_[w] = v;
    write_.store(next, std::memory_order_release);
    return true;
  }

  bool pop(T& out) {
    const size_t r = read_.load(std::memory_order_relaxed);
    if (r == write_.load(std::memory_order_acquire)) return false;     // empty
    out = buf_[r];
    read_.store((r + 1) & mask_, std::memory_order_release);
    return true;
  }

 private:
  std::vector<T> buf_;
  size_t mask_;
  alignas(64) std::atomic<size_t> write_{0};
  alignas(64) std::atomic<size_t> read_{0};
};

}  // namespace ld
