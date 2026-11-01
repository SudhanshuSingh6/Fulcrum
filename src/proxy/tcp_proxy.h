#pragma once

#include <memory>
#include <vector>

#include "balancing/round_robin.h"
#include "control/args.h"
#include "core/fd.h"

namespace fulcrum {

// The backends and the round-robin position. Shared (via shared_ptr) with every
// connection thread, so it stays alive as long as any thread still uses it.
struct BackendPool {
  explicit BackendPool(std::vector<HostPort> b) : backends(std::move(b)), rr(backends.size()) {}
  const std::vector<HostPort> backends;
  RoundRobin rr;
};

// The TCP proxy: owns the listening socket and the backend pool.
class TcpProxy {
 public:
  TcpProxy(Fd listener, std::vector<HostPort> backends);

  // The accept loop. Runs until a fatal accept error. Each client gets its own
  // detached thread, which picks a backend (round-robin with failover) and relays.
  void run();

 private:
  Fd listener_;
  std::shared_ptr<BackendPool> pool_;
};

}  // namespace fulcrum
