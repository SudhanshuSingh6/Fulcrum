#include "core/io.h"

#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <thread>
#include <vector>

#include "core/fd.h"

namespace {

struct SocketPair {
  fulcrum::Fd a, b;
};

SocketPair make_pair() {
  int fds[2];
  EXPECT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds), 0);
  return {fulcrum::Fd(fds[0]), fulcrum::Fd(fds[1])};
}

char pattern(std::size_t i) { return static_cast<char>((i * 31) % 251); }

}  // namespace

TEST(WriteAll, SendsEverythingThroughASmallBuffer) {
  auto [writer, reader] = make_pair();
  // A tiny send buffer forces send() to accept only part of each call.
  const int small = 4096;
  ASSERT_EQ(::setsockopt(writer.get(), SOL_SOCKET, SO_SNDBUF, &small, sizeof(small)), 0);

  constexpr std::size_t kSize = 4 * 1024 * 1024;
  std::vector<char> data(kSize);
  for (std::size_t i = 0; i < kSize; ++i) data[i] = pattern(i);

  std::vector<char> received;
  received.reserve(kSize);
  std::thread drain([&] {
    char buf[1000];
    int reads = 0;
    for (;;) {
      const ssize_t n = ::recv(reader.get(), buf, sizeof(buf), 0);
      if (n <= 0) break;
      received.insert(received.end(), buf, buf + n);
      // Read slowly now and then so the writer's buffer fills up.
      if (++reads % 512 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });

  const fulcrum::IoResult r = fulcrum::write_all(writer.get(), data);
  ::shutdown(writer.get(), SHUT_WR);
  drain.join();

  EXPECT_TRUE(r.ok) << r.err;
  EXPECT_EQ(r.written, kSize);
  ASSERT_EQ(received.size(), kSize);
  EXPECT_TRUE(received == data) << "bytes arrived out of order or corrupted";
}

TEST(WriteAll, ClosedPeerGivesEpipeNotSigpipe) {
  auto [writer, reader] = make_pair();
  reader.reset();  // close the other end
  const std::vector<char> data(1024, 'x');
  // Without MSG_NOSIGNAL this would kill the test process with SIGPIPE.
  const fulcrum::IoResult r = fulcrum::write_all(writer.get(), data);
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.err, EPIPE);
}

TEST(WriteAll, EmptyBufferIsOk) {
  auto [writer, reader] = make_pair();
  const fulcrum::IoResult r = fulcrum::write_all(writer.get(), {});
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.written, 0u);
}
