#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fulcrum {

struct HostPort {
  std::string host;  // IPv4 dotted address, e.g. "127.0.0.1"
  std::uint16_t port = 0;

  std::string to_string() const { return host + ":" + std::to_string(port); }
  bool operator==(const HostPort&) const = default;
};

// Parses "a.b.c.d:port". `allow_port_zero` is for listen addresses, where 0
// asks the kernel to pick a free port.
std::optional<HostPort> parse_host_port(std::string_view text, bool allow_port_zero,
                                        std::string& error);

struct Args {
  HostPort listen;
  std::vector<HostPort> backends;
};

struct ParseResult {
  enum class Kind { kRun, kHelp, kVersion, kError };
  Kind kind = Kind::kError;
  Args args;          // valid when kind == kRun
  std::string error;  // set when kind == kError
};

ParseResult parse_args(int argc, const char* const* argv);

const char* usage_text();

}  // namespace fulcrum
