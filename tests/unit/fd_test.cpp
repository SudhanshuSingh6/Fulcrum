#include "core/fd.h"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <utility>

namespace {

bool is_open(int fd) { return ::fcntl(fd, F_GETFD) != -1; }

int new_fd() {
  int fds[2];
  EXPECT_EQ(::pipe2(fds, O_CLOEXEC), 0);
  ::close(fds[1]);
  return fds[0];
}

}  // namespace

TEST(Fd, DefaultIsInvalid) {
  fulcrum::Fd fd;
  EXPECT_FALSE(fd.valid());
  EXPECT_EQ(fd.get(), -1);
}

TEST(Fd, ClosesOnDestruction) {
  const int raw = new_fd();
  { fulcrum::Fd fd(raw); }
  EXPECT_FALSE(is_open(raw));
}

TEST(Fd, MoveConstructTransfersOwnership) {
  const int raw = new_fd();
  fulcrum::Fd a(raw);
  fulcrum::Fd b(std::move(a));
  EXPECT_FALSE(a.valid());  // NOLINT(bugprone-use-after-move): checking moved-from state
  EXPECT_EQ(b.get(), raw);
  EXPECT_TRUE(is_open(raw));
}

TEST(Fd, MoveAssignClosesOldFd) {
  const int raw1 = new_fd();
  const int raw2 = new_fd();
  fulcrum::Fd a(raw1);
  fulcrum::Fd b(raw2);
  b = std::move(a);
  EXPECT_FALSE(is_open(raw2));
  EXPECT_TRUE(is_open(raw1));
  EXPECT_EQ(b.get(), raw1);
}

TEST(Fd, ReleaseDoesNotClose) {
  const int raw = new_fd();
  int released = -1;
  {
    fulcrum::Fd fd(raw);
    released = fd.release();
    EXPECT_FALSE(fd.valid());
  }
  EXPECT_EQ(released, raw);
  EXPECT_TRUE(is_open(raw));
  ::close(raw);
}

TEST(Fd, ResetClosesAndAdopts) {
  const int raw1 = new_fd();
  const int raw2 = new_fd();
  fulcrum::Fd fd(raw1);
  fd.reset(raw2);
  EXPECT_FALSE(is_open(raw1));
  EXPECT_EQ(fd.get(), raw2);
}
