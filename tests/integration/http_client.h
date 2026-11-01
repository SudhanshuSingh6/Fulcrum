#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace fulcrum::test {

// Connects a blocking TCP socket to 127.0.0.1:port. Returns the fd, or -1.
int connect_loopback(std::uint16_t port);

struct HttpResponse {
  int status = 0;
  std::string body;
};

// Minimal blocking HTTP/1.1 client for tests. Sends `Connection: close` and
// reads until EOF, so it doesn't need to understand Content-Length or chunking.
// Returns nullopt on a socket error or an unparsable status line.
std::optional<HttpResponse> http_request(std::uint16_t port, const std::string& method,
                                         const std::string& path, const std::string& body = "");

// Splits a raw "HTTP/1.x NNN ...\r\n...\r\n\r\nbody" into status + body.
std::optional<HttpResponse> parse_response(const std::string& raw);

inline std::optional<HttpResponse> http_get(std::uint16_t port, const std::string& path) {
  return http_request(port, "GET", path);
}

}  // namespace fulcrum::test
