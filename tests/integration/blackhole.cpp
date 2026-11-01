#include "blackhole.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>

namespace fulcrum::test {

namespace {

sockaddr_in loopback(std::uint16_t port) {
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons(port);
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  return a;
}

}  // namespace

Blackhole::Blackhole() {
  listener_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  sockaddr_in addr = loopback(0);
  socklen_t len = sizeof(addr);
  if (listener_ < 0 || ::bind(listener_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
      ::listen(listener_, 0) != 0 ||
      ::getsockname(listener_, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
    std::abort();
  }
  port_ = ntohs(addr.sin_port);

  // Connect until one attempt doesn't complete within 200 ms: the queue is full.
  for (int i = 0; i < 16; ++i) {
    const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    fillers_.push_back(fd);
    sockaddr_in to = loopback(port_);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&to), sizeof(to)) == 0) continue;
    if (errno != EINPROGRESS) std::abort();
    pollfd p{fd, POLLOUT, 0};
    if (::poll(&p, 1, 200) == 0) return;  // this one hangs: blackhole ready
  }
  std::abort();  // never filled up; the test would be meaningless
}

Blackhole::~Blackhole() {
  for (const int fd : fillers_) ::close(fd);
  ::close(listener_);
}

}  // namespace fulcrum::test
