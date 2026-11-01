#include "core/io.h"

#include <sys/socket.h>

#include <cerrno>

namespace fulcrum {

IoResult write_all(int fd, std::span<const char> data) {
  IoResult result;
  while (!data.empty()) {
    const ssize_t n = ::send(fd, data.data(), data.size(), MSG_NOSIGNAL);
    if (n < 0) {
      if (errno == EINTR) continue;
      result.ok = false;
      result.err = errno;
      return result;
    }
    const auto sent = static_cast<std::size_t>(n);
    result.written += sent;
    data = data.subspan(sent);
  }
  return result;
}

}  // namespace fulcrum
