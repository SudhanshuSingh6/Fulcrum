#pragma once

#include <cstddef>
#include <span>

namespace fulcrum {

struct IoResult {
  bool ok = true;
  int err = 0;              // errno when !ok
  std::size_t written = 0;  // bytes written before success or failure
};

// Writes all of `data` to a blocking socket. send() may write only part of
// the buffer (a "partial write"), so this loops until everything is sent.
// Retries on EINTR. Uses MSG_NOSIGNAL so a closed peer gives EPIPE instead of
// killing the process with SIGPIPE.
IoResult write_all(int fd, std::span<const char> data);

}  // namespace fulcrum
