#include "proxy/tcp_proxy.h"

#include <spdlog/spdlog.h>
#include <sys/socket.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <thread>
#include <utility>

namespace fulcrum {

namespace {

enum class AcceptError {
  kRetry,       // interrupted; try again right away
  kSkipClient,  // problem with one pending client; move on to the next
  kBackoff,     // out of fds/memory; wait a little, then try again
  kFatal,       // our listener is broken; stop
};

// Sorts accept4() errors as described in `man 2 accept4` (ERRORS section).
AcceptError classify_accept_error(int err) {
  switch (err) {
    case EINTR:
      return AcceptError::kRetry;
    case ECONNABORTED:  // client reset the connection before we accepted it
    case EPROTO:
    // Linux passes pending network errors of the new socket to accept(); the
    // man page says to treat these like EAGAIN and retry.
    case ENETDOWN:
    case ENOPROTOOPT:
    case EHOSTDOWN:
    case ENONET:
    case EHOSTUNREACH:
    case EOPNOTSUPP:
    case ENETUNREACH:
      return AcceptError::kSkipClient;
    case EMFILE:  // per-process fd limit
    case ENFILE:  // system-wide fd limit
    case ENOBUFS:
    case ENOMEM:
      return AcceptError::kBackoff;
    default:  // EBADF, EINVAL, ENOTSOCK, EFAULT, ...: a bug, not a client problem
      return AcceptError::kFatal;
  }
}

constexpr auto kResourceBackoff = std::chrono::milliseconds(100);

}  // namespace

TcpProxy::TcpProxy(Fd listener, std::vector<HostPort> backends)
    : listener_(std::move(listener)), backends_(std::move(backends)) {}

void TcpProxy::run() {
  for (;;) {
    Fd client(::accept4(listener_.get(), nullptr, nullptr, SOCK_CLOEXEC));
    if (client) {
      // Phase 1: accept and close (the Fd destructor closes it).
      // Phase 2 hands the client to a Connection instead.
      continue;
    }

    const int err = errno;
    switch (classify_accept_error(err)) {
      case AcceptError::kRetry:
        break;
      case AcceptError::kSkipClient:
        spdlog::debug("accept: skipped a client: {}", std::strerror(err));
        break;
      case AcceptError::kBackoff:
        // The pending connection stays in the backlog, so retrying at once
        // would fail again and spin at 100% CPU.
        spdlog::warn("accept: {}; retrying in {} ms", std::strerror(err), kResourceBackoff.count());
        std::this_thread::sleep_for(kResourceBackoff);
        break;
      case AcceptError::kFatal:
        spdlog::error("accept: {}; stopping", std::strerror(err));
        return;
    }
  }
}

}  // namespace fulcrum
