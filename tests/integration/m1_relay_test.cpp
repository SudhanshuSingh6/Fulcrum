// M1 phase 2: Fulcrum relays to one backend.
// Covers M1 acceptance criteria 2 (100 MB echo), 5 (half-close), 6 (200 concurrent).
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <latch>
#include <string>
#include <thread>
#include <vector>

#include "http_client.h"
#include "proxy_fixture.h"

using fulcrum::test::connect_loopback;
using fulcrum::test::http_get;
using fulcrum::test::parse_response;
using fulcrum::test::ProxyFixture;

namespace {

// Deterministic pseudo-random bytes (xorshift64). Two generators with the same
// seed produce the same stream, so the receiver can check every byte without
// keeping a copy of what was sent.
class ByteStream {
 public:
  explicit ByteStream(std::uint64_t seed) : state_(seed) {}
  void fill(char* out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
      state_ ^= state_ << 13;
      state_ ^= state_ >> 7;
      state_ ^= state_ << 17;
      out[i] = static_cast<char>(state_ & 0xff);
    }
  }

 private:
  std::uint64_t state_;
};

bool send_all(int fd, const char* data, std::size_t len) {
  while (len > 0) {
    const ssize_t n = ::send(fd, data, len, MSG_NOSIGNAL);
    if (n <= 0) return false;
    data += n;
    len -= static_cast<std::size_t>(n);
  }
  return true;
}

std::string recv_all(int fd) {
  std::string out;
  char buf[16 * 1024];
  for (;;) {
    const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) break;
    out.append(buf, static_cast<std::size_t>(n));
  }
  return out;
}

}  // namespace

TEST(M1Relay, GetThroughProxy) {
  ProxyFixture fx(1);
  ASSERT_TRUE(fx.start_fulcrum());
  const auto resp = http_get(fx.fulcrum_port(), "/");
  ASSERT_TRUE(resp.has_value());
  EXPECT_EQ(resp->status, 200);
  EXPECT_EQ(resp->body, "a");
}

// AC2: a 100 MB upload to /echo comes back byte-identical.
TEST(M1Relay, Echo100MegabytesIsByteIdentical) {
  constexpr std::size_t kSize = 100'000'000;
  constexpr std::uint64_t kSeed = 0x5eed'f00d'cafe'babeULL;
  ProxyFixture fx(1);
  ASSERT_TRUE(fx.start_fulcrum());

  const int fd = connect_loopback(fx.fulcrum_port());
  ASSERT_GE(fd, 0);
  const std::string head =
      "POST /echo HTTP/1.1\r\nHost: test\r\nContent-Length: " + std::to_string(kSize) +
      "\r\nConnection: close\r\n\r\n";
  ASSERT_TRUE(send_all(fd, head.data(), head.size()));

  // Send and receive at the same time: echo replies while it is still reading,
  // so sending everything first would fill every buffer and deadlock.
  std::atomic<bool> send_ok{true};
  std::thread sender([&] {
    ByteStream gen(kSeed);
    std::vector<char> chunk(64 * 1024);
    for (std::size_t sent = 0; sent < kSize;) {
      const std::size_t n = std::min(chunk.size(), kSize - sent);
      gen.fill(chunk.data(), n);
      if (!send_all(fd, chunk.data(), n)) {
        send_ok = false;
        return;
      }
      sent += n;
    }
  });

  // Read the response headers.
  std::string headers;
  char c;
  while (headers.find("\r\n\r\n") == std::string::npos && ::recv(fd, &c, 1, 0) == 1) {
    headers += c;
  }
  ASSERT_EQ(headers.rfind("HTTP/1.1 200", 0), 0u) << headers;

  // Compare the body with a second generator using the same seed.
  ByteStream expected(kSeed);
  std::vector<char> got(64 * 1024), want(64 * 1024);
  std::size_t received = 0;
  bool match = true;
  for (;;) {
    const ssize_t n = ::recv(fd, got.data(), got.size(), 0);
    if (n <= 0) break;
    const auto len = static_cast<std::size_t>(n);
    if (received + len > kSize) {
      match = false;
      break;
    }
    expected.fill(want.data(), len);
    if (!std::equal(got.begin(), got.begin() + n, want.begin())) {
      match = false;
      break;
    }
    received += len;
  }
  sender.join();
  ::close(fd);

  EXPECT_TRUE(send_ok);
  EXPECT_TRUE(match) << "first mismatch near byte " << received;
  EXPECT_EQ(received, kSize);
}

// AC5: the client sends its request, shuts its write side, and still gets
// the complete response.
TEST(M1Relay, HalfCloseStillGetsFullResponse) {
  ProxyFixture fx(1);
  ASSERT_TRUE(fx.start_fulcrum());

  const int fd = connect_loopback(fx.fulcrum_port());
  ASSERT_GE(fd, 0);
  const std::string req = "GET / HTTP/1.1\r\nHost: test\r\nConnection: close\r\n\r\n";
  ASSERT_TRUE(send_all(fd, req.data(), req.size()));
  ASSERT_EQ(::shutdown(fd, SHUT_WR), 0);

  const auto resp = parse_response(recv_all(fd));
  ::close(fd);
  ASSERT_TRUE(resp.has_value());
  EXPECT_EQ(resp->status, 200);
  EXPECT_EQ(resp->body, "a");
}

// AC6: 200 concurrent connections all complete with correct responses.
TEST(M1Relay, TwoHundredConcurrentConnections) {
  constexpr int kClients = 200;
  ProxyFixture fx(1);
  ASSERT_TRUE(fx.start_fulcrum());

  std::atomic<int> ok{0};
  std::latch start(kClients);
  std::vector<std::thread> clients;
  for (int i = 0; i < kClients; ++i) {
    clients.emplace_back([&] {
      start.arrive_and_wait();  // all connect at about the same moment
      const auto resp = http_get(fx.fulcrum_port(), "/");
      if (resp && resp->status == 200 && resp->body == "a") ++ok;
    });
  }
  for (auto& t : clients) t.join();
  EXPECT_EQ(ok.load(), kClients);
}
