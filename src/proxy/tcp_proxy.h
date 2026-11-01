#pragma once

#include <vector>

#include "control/args.h"
#include "core/fd.h"

namespace fulcrum {

// The TCP proxy: owns the listening socket and the backend list.
class TcpProxy {
 public:
  TcpProxy(Fd listener, std::vector<HostPort> backends);

  // [human] The accept loop. Runs until a fatal error.
  //
  // Phase 1: accept each client with accept4(..., SOCK_CLOEXEC) and close it
  // right away. Retry on EINTR. For other errors, decide which ones are
  // transient (keep looping, maybe log) and which are fatal (log and return).
  // See the ERRORS section of `man 2 accept4`.
  void run();

 private:
  Fd listener_;
  std::vector<HostPort> backends_;
};

}  // namespace fulcrum
