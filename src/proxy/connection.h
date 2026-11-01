#pragma once

#include <cstdint>

#include "core/fd.h"

namespace fulcrum {

struct PumpResult {
  std::uint64_t bytes = 0;  // bytes copied from src to dst
  int err = 0;              // 0 = ended with a clean EOF; otherwise the errno
};

struct ConnectionResult {
  PumpResult upstream;    // client -> backend
  PumpResult downstream;  // backend -> client
};

// Copies bytes from `src` to `dst` until EOF or an error (SEMANTICS §2).
//   EOF on src  -> shutdown(dst, SHUT_WR): pass the half-close on.
//   any error   -> shutdown both sockets fully, which wakes the pump running
//                  in the other direction so the whole connection ends.
// Never closes either fd; the owner closes them after both pumps finish.
PumpResult pump(int src, int dst);

// Proxies one connection with a thread pair: a new thread pumps
// client -> backend while the calling thread pumps backend -> client.
// Returns when both directions are done; both sockets are then closed.
// Throws std::system_error if the second thread can't be created.
ConnectionResult relay(Fd client, Fd backend);

}  // namespace fulcrum
