// connect_tcp: success, refused, and timeout (via a blackholed port).
#include "core/socket.h"

#include <fcntl.h>
#include <gtest/gtest.h>

#include <chrono>

#include "blackhole.h"
#include "core/listener.h"
#include "core/socket_util.h"

using namespace std::chrono_literals;

namespace {

// A port nothing listens on: bind a socket to get a free port, then close it.
std::uint16_t closed_port() {
  std::string error;
  fulcrum::Fd l = fulcrum::listen_tcp({"127.0.0.1", 0}, 1, error);
  return fulcrum::local_port(l);
}

}  // namespace

TEST(ConnectTcp, ConnectsAndReturnsBlockingSocket) {
  std::string error;
  fulcrum::Fd listener = fulcrum::listen_tcp({"127.0.0.1", 0}, 4, error);
  ASSERT_TRUE(listener.valid()) << error;

  fulcrum::Fd fd = fulcrum::connect_tcp({"127.0.0.1", fulcrum::local_port(listener)}, 1s, error);
  ASSERT_TRUE(fd.valid()) << error;
  EXPECT_FALSE(::fcntl(fd.get(), F_GETFL) & O_NONBLOCK) << "relay threads need blocking sockets";
  EXPECT_TRUE(::fcntl(fd.get(), F_GETFD) & FD_CLOEXEC);
}

TEST(ConnectTcp, RefusedFailsFast) {
  std::string error;
  const auto start = std::chrono::steady_clock::now();
  fulcrum::Fd fd = fulcrum::connect_tcp({"127.0.0.1", closed_port()}, 1s, error);
  EXPECT_FALSE(fd.valid());
  EXPECT_LT(std::chrono::steady_clock::now() - start, 200ms);
  EXPECT_NE(error.find("refused"), std::string::npos) << error;
}

TEST(ConnectTcp, BlackholeTimesOut) {
  fulcrum::test::Blackhole hole;
  std::string error;
  const auto start = std::chrono::steady_clock::now();
  fulcrum::Fd fd = fulcrum::connect_tcp({"127.0.0.1", hole.port()}, 300ms, error);
  const auto took = std::chrono::steady_clock::now() - start;
  EXPECT_FALSE(fd.valid());
  EXPECT_GE(took, 290ms);
  EXPECT_LT(took, 600ms);
  EXPECT_NE(error.find("timed out"), std::string::npos) << error;
}
