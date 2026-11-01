// M1 phase 1: Fulcrum listens and keeps accepting.
// Tests the [human] listener and accept loop through the real binary.
#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>

#include "process.h"
#include "proxy_fixture.h"

using namespace std::chrono_literals;
using fulcrum::test::Process;
using fulcrum::test::ProxyFixture;

namespace {

bool connect_and_close(std::uint16_t port) {
  const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0) return false;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  const bool ok = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
  ::close(fd);
  return ok;
}

}  // namespace

TEST(M1Listen, PortAcceptsConnections) {
  ProxyFixture fx(1);
  ASSERT_TRUE(fx.start_fulcrum()) << "fulcrum did not open its port";
  EXPECT_TRUE(connect_and_close(fx.fulcrum_port()));
}

TEST(M1Listen, KeepsAcceptingAfterManyConnections) {
  ProxyFixture fx(1);
  ASSERT_TRUE(fx.start_fulcrum());
  for (int i = 0; i < 50; ++i) {
    ASSERT_TRUE(connect_and_close(fx.fulcrum_port())) << "connection " << i;
  }
  // Still running: the accept loop didn't exit.
  EXPECT_FALSE(fx.fulcrum().wait_exit(100ms).has_value());
}

TEST(M1Listen, BadArgumentsExitWithCode2) {
  Process p = Process::spawn(FULCRUM_BIN, {"--listen", "127.0.0.1:8080"});  // no --backend
  const auto status = p.wait_exit(5s);
  ASSERT_TRUE(status.has_value());
  ASSERT_TRUE(WIFEXITED(*status));
  EXPECT_EQ(WEXITSTATUS(*status), 2);
}
