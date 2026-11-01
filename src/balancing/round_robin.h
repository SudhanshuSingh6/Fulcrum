#pragma once

#include <atomic>
#include <cstddef>

namespace fulcrum {

// Hands out indexes 0, 1, ..., n-1, 0, 1, ... Safe to call from many threads.
class RoundRobin {
 public:
  explicit RoundRobin(std::size_t n) : n_(n) {}

  std::size_t next() noexcept {
    // fetch_add is one atomic read-modify-write, so two threads never get the
    // same counter value. Relaxed order is enough: the counter protects no
    // other data, we only need each value handed out once.
    return counter_.fetch_add(1, std::memory_order_relaxed) % n_;
  }

  std::size_t size() const noexcept { return n_; }

 private:
  const std::size_t n_;
  std::atomic<std::size_t> counter_{0};
};

}  // namespace fulcrum
