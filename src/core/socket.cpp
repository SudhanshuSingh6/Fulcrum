#include "core/socket.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstring>

namespace fulcrum {

namespace {

std::string describe(const HostPort& addr, int err) {
  return "connect " + addr.to_string() + ": " + std::strerror(err);
}

// Waits until fd is writable (= connect finished, successfully or not).
// Returns 1 if ready, 0 on timeout, -1 on error. Handles EINTR by waiting
// again for the remaining time only.
int wait_writable(int fd, std::chrono::steady_clock::time_point deadline) {
  for (;;) {
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now());
    if (left.count() <= 0) return 0;
    pollfd p{fd, POLLOUT, 0};
    const int rc = ::poll(&p, 1, static_cast<int>(left.count()));
    if (rc < 0 && errno == EINTR) continue;
    return rc;
  }
}

}  // namespace

Fd connect_tcp(const HostPort& addr, std::chrono::milliseconds timeout, std::string& error) {
  sockaddr_in sa{};
  sa.sin_family = AF_INET;
  sa.sin_port = htons(addr.port);
  if (::inet_pton(AF_INET, addr.host.c_str(), &sa.sin_addr) != 1) {
    error = "invalid IPv4 address '" + addr.host + "'";
    return Fd{};
  }

  // Non-blocking, so connect() returns at once instead of waiting for the
  // handshake (which can take minutes if SYNs are being dropped).
  Fd fd(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0));
  if (!fd) {
    error = std::string("socket: ") + std::strerror(errno);
    return Fd{};
  }

  const auto deadline = std::chrono::steady_clock::now() + timeout;
  if (::connect(fd.get(), reinterpret_cast<const sockaddr*>(&sa), sizeof(sa)) != 0) {
    // EINPROGRESS: handshake started. EINTR: same, it continues in the
    // background (man 2 connect). Anything else failed right away.
    if (errno != EINPROGRESS && errno != EINTR) {
      error = describe(addr, errno);
      return Fd{};
    }
    const int ready = wait_writable(fd.get(), deadline);
    if (ready == 0) {
      error = "connect " + addr.to_string() + ": timed out after " +
              std::to_string(timeout.count()) + " ms";
      return Fd{};
    }
    if (ready < 0) {
      error = std::string("poll: ") + std::strerror(errno);
      return Fd{};
    }
    // Writable means "finished", not "succeeded": SO_ERROR holds the result.
    int so_error = 0;
    socklen_t len = sizeof(so_error);
    if (::getsockopt(fd.get(), SOL_SOCKET, SO_ERROR, &so_error, &len) != 0) {
      error = std::string("getsockopt(SO_ERROR): ") + std::strerror(errno);
      return Fd{};
    }
    if (so_error != 0) {
      error = describe(addr, so_error);
      return Fd{};
    }
  }

  // M1's relay threads use blocking I/O, so switch back to blocking.
  const int flags = ::fcntl(fd.get(), F_GETFL);
  if (flags < 0 || ::fcntl(fd.get(), F_SETFL, flags & ~O_NONBLOCK) != 0) {
    error = std::string("fcntl: ") + std::strerror(errno);
    return Fd{};
  }
  return fd;
}

}  // namespace fulcrum
