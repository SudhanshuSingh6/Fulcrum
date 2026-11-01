#include "proxy/tcp_proxy.h"

#include <spdlog/spdlog.h>
#include <sys/socket.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

#include "core/socket.h"
#include "proxy/connection.h"

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

constexpr auto kConnectTimeout = std::chrono::milliseconds(1000);

// Tries each backend once, starting at `first`, and returns the first one
// that connects. Returns an invalid Fd if all of them fail.
Fd connect_with_failover(const BackendPool& pool, std::size_t first) {
  const std::size_t n = pool.backends.size();
  for (std::size_t i = 0; i < n; ++i) {
    const HostPort& addr = pool.backends[(first + i) % n];
    std::string error;
    Fd backend = connect_tcp(addr, kConnectTimeout, error);
    if (backend) return backend;
    spdlog::warn("backend {} failed: {}", addr.to_string(), error);
  }
  return Fd{};
}

// Runs on the connection's own thread: pick a backend, then relay.
void handle_client(Fd client, std::shared_ptr<BackendPool> pool, std::size_t first) {
  Fd backend = connect_with_failover(*pool, first);
  if (!backend) {
    spdlog::warn("outcome=no_backend: all {} backends failed", pool->backends.size());
    return;  // client is closed when it goes out of scope
  }
  try {
    relay(std::move(client), std::move(backend));
  } catch (const std::system_error& e) {
    spdlog::warn("cannot start relay thread: {}", e.what());
  }
}

}  // namespace

TcpProxy::TcpProxy(Fd listener, std::vector<HostPort> backends)
    : listener_(std::move(listener)), pool_(std::make_shared<BackendPool>(std::move(backends))) {}

void TcpProxy::run() {
  for (;;) {
    Fd client(::accept4(listener_.get(), nullptr, nullptr, SOCK_CLOEXEC));
    if (client) {
      // Pick the starting backend here, in accept order, so sequential clients
      // go a, b, c, a, ... exactly. Failover continues from this index.
      const std::size_t first = pool_->rr.next();
      try {
        std::thread(handle_client, std::move(client), pool_, first).detach();
      } catch (const std::system_error& e) {
        // Thread limit reached. The client was moved into the failed thread's
        // arguments and is closed with them; keep accepting others.
        spdlog::warn("cannot start connection thread: {}", e.what());
      }
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
