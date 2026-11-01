#include "http_client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <string_view>

namespace fulcrum::test {

namespace {

// Closes the fd on every return path.
struct FdGuard {
  int fd;
  ~FdGuard() {
    if (fd >= 0) ::close(fd);
  }
};

bool write_all(int fd, std::string_view data) {
  while (!data.empty()) {
    const ssize_t n = ::send(fd, data.data(), data.size(), MSG_NOSIGNAL);
    if (n < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    data.remove_prefix(static_cast<size_t>(n));
  }
  return true;
}

}  // namespace

int connect_loopback(std::uint16_t port) {
  const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0) return -1;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(fd);
    return -1;
  }
  return fd;
}

std::optional<HttpResponse> parse_response(const std::string& raw) {
  HttpResponse resp;
  if (std::sscanf(raw.c_str(), "HTTP/1.%*d %d", &resp.status) != 1) return std::nullopt;
  const auto header_end = raw.find("\r\n\r\n");
  if (header_end == std::string::npos) return std::nullopt;
  resp.body = raw.substr(header_end + 4);
  return resp;
}

std::optional<HttpResponse> http_request(std::uint16_t port, const std::string& method,
                                         const std::string& path, const std::string& body) {
  FdGuard sock{connect_loopback(port)};
  if (sock.fd < 0) return std::nullopt;

  std::string req = method + " " + path + " HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n";
  if (!body.empty() || method == "POST") {
    req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  }
  req += "\r\n";
  req += body;
  if (!write_all(sock.fd, req)) return std::nullopt;

  std::string raw;
  char buf[16 * 1024];
  for (;;) {
    const ssize_t n = ::recv(sock.fd, buf, sizeof(buf), 0);
    if (n == 0) break;
    if (n < 0) {
      if (errno == EINTR) continue;
      return std::nullopt;
    }
    raw.append(buf, static_cast<size_t>(n));
  }

  return parse_response(raw);
}

}  // namespace fulcrum::test
