#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace fulcrum::test {

struct HttpResponse {
  int status = 0;
  std::string body;
};

// Minimal blocking HTTP/1.1 client for tests. Sends `Connection: close` and
// reads until EOF, so it doesn't need to understand Content-Length or chunking.
// Returns nullopt on a socket error or an unparsable status line.
std::optional<HttpResponse> http_request(std::uint16_t port, const std::string& method,
                                         const std::string& path, const std::string& body = "");

inline std::optional<HttpResponse> http_get(std::uint16_t port, const std::string& path) {
  return http_request(port, "GET", path);
}

}  // namespace fulcrum::test
