#include "core/socket_util.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace fulcrum {

std::uint16_t local_port(const Fd& fd) {
  sockaddr_in addr{};
  socklen_t len = sizeof(addr);
  if (::getsockname(fd.get(), reinterpret_cast<sockaddr*>(&addr), &len) != 0) return 0;
  return ntohs(addr.sin_port);
}

}  // namespace fulcrum
