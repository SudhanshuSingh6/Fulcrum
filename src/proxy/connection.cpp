#include "proxy/connection.h"

#include <sys/socket.h>

#include <array>
#include <cerrno>
#include <span>
#include <thread>

#include "core/io.h"

namespace fulcrum {

namespace {
constexpr std::size_t kBufferSize = 64 * 1024;
}

PumpResult pump(int src, int dst) {
  PumpResult result;
  std::array<char, kBufferSize> buf;

  for (;;) {
    const ssize_t n = ::recv(src, buf.data(), buf.size(), 0);
    if (n > 0) {
      const auto len = static_cast<std::size_t>(n);
      const IoResult w = write_all(dst, std::span<const char>(buf.data(), len));
      result.bytes += w.written;
      if (!w.ok) {
        result.err = w.err;
        break;
      }
      continue;
    }
    if (n == 0) {
      // Clean EOF: src won't send more. Tell dst the same (FIN), but keep the
      // other direction open: dst may still be sending its reply.
      // ENOTCONN here just means dst is already gone; nothing to do.
      ::shutdown(dst, SHUT_WR);
      return result;
    }
    if (errno == EINTR) continue;
    result.err = errno;
    break;
  }

  // Error: end the whole connection. shutdown (not close) wakes the other
  // pump, which may be blocked in recv() or send() on these fds, without
  // freeing the fd numbers while it still uses them.
  ::shutdown(src, SHUT_RDWR);
  ::shutdown(dst, SHUT_RDWR);
  return result;
}

ConnectionResult relay(Fd client, Fd backend) {
  ConnectionResult result;
  std::thread upstream([&] { result.upstream = pump(client.get(), backend.get()); });
  result.downstream = pump(backend.get(), client.get());
  upstream.join();
  return result;
  // client and backend are closed here, after both pumps have finished.
}

}  // namespace fulcrum
