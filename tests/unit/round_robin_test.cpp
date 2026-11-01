#include "balancing/round_robin.h"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <thread>
#include <vector>

TEST(RoundRobin, CyclesInOrder) {
  fulcrum::RoundRobin rr(3);
  const std::vector<std::size_t> want{0, 1, 2, 0, 1, 2, 0};
  for (const auto w : want) EXPECT_EQ(rr.next(), w);
}

TEST(RoundRobin, SingleBackendAlwaysZero) {
  fulcrum::RoundRobin rr(1);
  for (int i = 0; i < 5; ++i) EXPECT_EQ(rr.next(), 0u);
}

TEST(RoundRobin, ConcurrentCallersGetAnEvenSplit) {
  // 8 threads x 3000 calls over 3 backends: if no value is ever handed out
  // twice or skipped, each backend gets exactly 8000.
  fulcrum::RoundRobin rr(3);
  std::array<std::atomic<int>, 3> counts{};
  std::vector<std::thread> threads;
  for (int t = 0; t < 8; ++t) {
    threads.emplace_back([&] {
      for (int i = 0; i < 3000; ++i) ++counts[rr.next()];
    });
  }
  for (auto& t : threads) t.join();
  for (const auto& c : counts) EXPECT_EQ(c.load(), 8000);
}
