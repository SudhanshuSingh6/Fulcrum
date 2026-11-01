#pragma once

#include <string>

#include "control/args.h"
#include "core/fd.h"

namespace fulcrum {

// [human] Creates a TCP socket listening on `addr` (IPv4).
//
// Requirements (checked by tests/unit/listener_test.cpp):
//   - the socket is close-on-exec (SOCK_CLOEXEC)
//   - SO_REUSEADDR is set, so Fulcrum can restart while old connections sit in TIME_WAIT
//   - SO_REUSEPORT is NOT set, so a second listener on the same port fails
//   - port 0 lets the kernel choose a free port
//
// On failure returns an invalid Fd and puts a readable message in `error`
// (which call failed, plus strerror(errno)).
Fd listen_tcp(const HostPort& addr, int backlog, std::string& error);

}  // namespace fulcrum
