#pragma once

#include <chrono>
#include <string>

#include "control/args.h"
#include "core/fd.h"

namespace fulcrum {

// Opens a TCP connection to `addr`, giving up after `timeout`.
// Uses a non-blocking connect() + poll(), then switches the socket back to
// blocking mode before returning it. On failure returns an invalid Fd and sets
// `error` (e.g. "connect 10.0.0.1:80: timed out after 1000 ms").
Fd connect_tcp(const HostPort& addr, std::chrono::milliseconds timeout, std::string& error);

}  // namespace fulcrum
