// Tests for listen_tcp ([human] code in src/core/listener.cpp).
#include "core/listener.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "core/socket_util.h"

namespace {

const fulcrum::HostPort kAnyLoopbackPort{"127.0.0.1", 0};

bool can_connect(std::uint16_t port) {
  fulcrum::Fd s(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  return ::connect(s.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
}

}  // namespace

TEST(Listener, ListensOnKernelChosenPort) {
  std::string error;
  fulcrum::Fd fd = fulcrum::listen_tcp(kAnyLoopbackPort, 16, error);
  ASSERT_TRUE(fd.valid()) << error;
  EXPECT_GT(fulcrum::local_port(fd), 0);
}

TEST(Listener, AcceptsClientConnections) {
  std::string error;
  fulcrum::Fd fd = fulcrum::listen_tcp(kAnyLoopbackPort, 16, error);
  ASSERT_TRUE(fd.valid()) << error;
  // The kernel completes the handshake into the backlog even before accept().
  EXPECT_TRUE(can_connect(fulcrum::local_port(fd)));
}

TEST(Listener, IsCloseOnExec) {
  std::string error;
  fulcrum::Fd fd = fulcrum::listen_tcp(kAnyLoopbackPort, 16, error);
  ASSERT_TRUE(fd.valid()) << error;
  EXPECT_TRUE(::fcntl(fd.get(), F_GETFD) & FD_CLOEXEC);
}

TEST(Listener, SetsReuseAddrButNotReusePort) {
  std::string error;
  fulcrum::Fd fd = fulcrum::listen_tcp(kAnyLoopbackPort, 16, error);
  ASSERT_TRUE(fd.valid()) << error;
  int value = 0;
  socklen_t len = sizeof(value);
  ASSERT_EQ(::getsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, &value, &len), 0);
  EXPECT_NE(value, 0) << "SO_REUSEADDR should be set";
  ASSERT_EQ(::getsockopt(fd.get(), SOL_SOCKET, SO_REUSEPORT, &value, &len), 0);
  EXPECT_EQ(value, 0) << "SO_REUSEPORT should not be set (that's M5)";
}

TEST(Listener, SecondListenerOnSamePortFails) {
  std::string error;
  fulcrum::Fd first = fulcrum::listen_tcp(kAnyLoopbackPort, 16, error);
  ASSERT_TRUE(first.valid()) << error;

  std::string error2;
  fulcrum::Fd second = fulcrum::listen_tcp({"127.0.0.1", fulcrum::local_port(first)}, 16, error2);
  EXPECT_FALSE(second.valid());
  EXPECT_FALSE(error2.empty());
}

TEST(Listener, UnassignedAddressFails) {
  // 192.0.2.0/24 is reserved for documentation (RFC 5737); no interface has it.
  std::string error;
  fulcrum::Fd fd = fulcrum::listen_tcp({"192.0.2.1", 0}, 16, error);
  EXPECT_FALSE(fd.valid());
  EXPECT_FALSE(error.empty());
}
