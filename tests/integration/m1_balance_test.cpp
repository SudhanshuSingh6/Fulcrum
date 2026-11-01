// M1 phase 3: round-robin, connect timeout, failover.
// Covers M1 acceptance criteria 1, 3, 4, 4b, and 6 with three backends.
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <latch>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "blackhole.h"
#include "http_client.h"
#include "process.h"
#include "proxy_fixture.h"

using namespace std::chrono_literals;
using fulcrum::test::Blackhole;
using fulcrum::test::connect_loopback;
using fulcrum::test::free_port;
using fulcrum::test::http_get;
using fulcrum::test::ProxyFixture;

namespace {

// Connects through Fulcrum, sends a request, and measures how long until the
// connection is closed. Returns the time, and whether any bytes came back.
struct CloseTiming {
  std::chrono::steady_clock::duration elapsed;
  bool got_data = false;
};

CloseTiming time_until_closed(std::uint16_t port) {
  const auto start = std::chrono::steady_clock::now();
  const int fd = connect_loopback(port);
  EXPECT_GE(fd, 0);
  const std::string req = "GET / HTTP/1.1\r\nHost: t\r\nConnection: close\r\n\r\n";
  ::send(fd, req.data(), req.size(), MSG_NOSIGNAL);
  CloseTiming t;
  char buf[256];
  for (;;) {
    const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) break;
    t.got_data = true;
  }
  t.elapsed = std::chrono::steady_clock::now() - start;
  ::close(fd);
  return t;
}

double seconds(std::chrono::steady_clock::duration d) {
  return std::chrono::duration<double>(d).count();
}

}  // namespace

// AC1: 9 sequential requests over 3 backends hit each exactly 3 times.
TEST(M1Balance, RoundRobinSplitsEvenly) {
  ProxyFixture fx(3);
  ASSERT_TRUE(fx.start_fulcrum());
  std::map<std::string, int> hits;
  for (int i = 0; i < 9; ++i) {
    const auto resp = http_get(fx.fulcrum_port(), "/");
    ASSERT_TRUE(resp.has_value()) << "request " << i;
    ++hits[resp->body];
  }
  EXPECT_EQ(hits, (std::map<std::string, int>{{"a", 3}, {"b", 3}, {"c", 3}}));
}

// AC3: with backend b killed, 30 requests all succeed (served by a and c).
TEST(M1Balance, DeadBackendIsSkipped) {
  ProxyFixture fx(3);
  fx.kill_backend(1);  // "b"
  ASSERT_TRUE(fx.start_fulcrum());
  std::map<std::string, int> hits;
  for (int i = 0; i < 30; ++i) {
    const auto resp = http_get(fx.fulcrum_port(), "/");
    ASSERT_TRUE(resp.has_value()) << "request " << i;
    EXPECT_EQ(resp->status, 200);
    ++hits[resp->body];
  }
  EXPECT_EQ(hits.count("b"), 0u);
  EXPECT_EQ(hits["a"] + hits["c"], 30);
}

// AC4: all backends refuse -> the client is closed within 1.5 s and the log
// shows no_backend.
TEST(M1Balance, AllRefusedClosesQuickly) {
  ProxyFixture fx(0);
  ASSERT_TRUE(fx.start_fulcrum_with_backends({free_port(), free_port(), free_port()}));
  const auto t = time_until_closed(fx.fulcrum_port());
  EXPECT_FALSE(t.got_data);
  EXPECT_LT(seconds(t.elapsed), 1.5);
  EXPECT_TRUE(fx.wait_for_log("no_backend", 1s)) << fx.fulcrum_log();
}

// AC4b: all 3 backends blackholed -> each gets the 1 s connect timeout, so the
// client is closed after about 3 s, and within 3.5 s.
TEST(M1Balance, AllBlackholedClosesAfterTimeouts) {
  Blackhole h1, h2, h3;
  ProxyFixture fx(0);
  ASSERT_TRUE(fx.start_fulcrum_with_backends({h1.port(), h2.port(), h3.port()}));
  const auto t = time_until_closed(fx.fulcrum_port());
  EXPECT_FALSE(t.got_data);
  EXPECT_GE(seconds(t.elapsed), 2.9) << "timeouts shorter than 1 s per backend?";
  EXPECT_LT(seconds(t.elapsed), 3.5);
  EXPECT_TRUE(fx.wait_for_log("no_backend", 1s)) << fx.fulcrum_log();
}

// Failover order: a blackholed first backend costs one timeout, then the
// next backend serves the request.
TEST(M1Balance, FailsOverAfterTimeout) {
  Blackhole hole;
  ProxyFixture fx(1);  // "a"
  ASSERT_TRUE(fx.start_fulcrum_with_backends({hole.port(), fx.backend_port(0)}));
  const auto start = std::chrono::steady_clock::now();
  const auto resp = http_get(fx.fulcrum_port(), "/");  // first pick = the blackhole
  const double took = seconds(std::chrono::steady_clock::now() - start);
  ASSERT_TRUE(resp.has_value());
  EXPECT_EQ(resp->body, "a");
  EXPECT_GE(took, 0.9);
  EXPECT_LT(took, 1.5);
}

// AC6 with three backends: 200 concurrent connections all succeed.
TEST(M1Balance, TwoHundredConcurrentOverThreeBackends) {
  constexpr int kClients = 200;
  ProxyFixture fx(3);
  ASSERT_TRUE(fx.start_fulcrum());
  std::atomic<int> ok{0};
  std::mutex mu;
  std::map<std::string, int> hits;
  std::latch start(kClients);
  std::vector<std::thread> clients;
  for (int i = 0; i < kClients; ++i) {
    clients.emplace_back([&] {
      start.arrive_and_wait();
      const auto resp = http_get(fx.fulcrum_port(), "/");
      if (!resp || resp->status != 200) return;
      ++ok;
      std::lock_guard lock(mu);
      ++hits[resp->body];
    });
  }
  for (auto& t : clients) t.join();
  EXPECT_EQ(ok.load(), kClients);
  // Each backend's share is exact only if accepts happen in order; just check
  // that all three were used and nothing unexpected came back.
  EXPECT_EQ(hits.size(), 3u);
  EXPECT_EQ(hits["a"] + hits["b"] + hits["c"], kClients);
}
