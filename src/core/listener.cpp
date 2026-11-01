#include "core/listener.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstring>

namespace fulcrum {

namespace {

// Builds "<what> <host:port>: <strerror>". Takes errno as a value because
// building strings can itself change errno.
std::string describe(const char* what, const HostPort& addr, int err) {
  return std::string(what) + " " + addr.to_string() + ": " + std::strerror(err);
}

}  // namespace

Fd listen_tcp(const HostPort& addr, int backlog, std::string& error) {
  Fd fd(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
  if (!fd) {
    error = describe("socket", addr, errno);
    return Fd{};
  }

  const int one = 1;
  if (::setsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0) {
    error = describe("setsockopt(SO_REUSEADDR)", addr, errno);
    return Fd{};
  }

  sockaddr_in sa{};
  sa.sin_family = AF_INET;
  sa.sin_port = htons(addr.port);
  if (::inet_pton(AF_INET, addr.host.c_str(), &sa.sin_addr) != 1) {
    error = "invalid IPv4 address '" + addr.host + "'";
    return Fd{};
  }

  if (::bind(fd.get(), reinterpret_cast<const sockaddr*>(&sa), sizeof(sa)) != 0) {
    error = describe("bind", addr, errno);
    return Fd{};
  }

  if (::listen(fd.get(), backlog) != 0) {
    error = describe("listen", addr, errno);
    return Fd{};
  }

  return fd;
}

}  // namespace fulcrum
