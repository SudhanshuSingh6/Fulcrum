#include "control/args.h"

#include <arpa/inet.h>

#include <charconv>

namespace fulcrum {

std::optional<HostPort> parse_host_port(std::string_view text, bool allow_port_zero,
                                        std::string& error) {
  const auto colon = text.rfind(':');
  if (colon == std::string_view::npos) {
    error = "expected host:port, got '" + std::string(text) + "'";
    return std::nullopt;
  }
  const std::string host(text.substr(0, colon));
  const std::string_view port_text = text.substr(colon + 1);

  in_addr ignored{};
  if (::inet_pton(AF_INET, host.c_str(), &ignored) != 1) {
    error = "invalid IPv4 address '" + host + "'";
    return std::nullopt;
  }

  unsigned value = 0;
  const auto [end, ec] =
      std::from_chars(port_text.data(), port_text.data() + port_text.size(), value);
  if (port_text.empty() || ec != std::errc{} || end != port_text.data() + port_text.size() ||
      value > 65535 || (value == 0 && !allow_port_zero)) {
    error = "invalid port '" + std::string(port_text) + "'";
    return std::nullopt;
  }
  return HostPort{host, static_cast<std::uint16_t>(value)};
}

ParseResult parse_args(int argc, const char* const* argv) {
  ParseResult result;
  bool have_listen = false;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      result.kind = ParseResult::Kind::kHelp;
      return result;
    }
    if (arg == "--version") {
      result.kind = ParseResult::Kind::kVersion;
      return result;
    }
    if (arg != "--listen" && arg != "--backend") {
      result.error = "unknown argument '" + std::string(arg) + "'";
      return result;
    }
    if (i + 1 >= argc) {
      result.error = std::string(arg) + " needs a value";
      return result;
    }
    const bool is_listen = arg == "--listen";
    auto hp = parse_host_port(argv[++i], /*allow_port_zero=*/is_listen, result.error);
    if (!hp) {
      result.error = std::string(arg) + ": " + result.error;
      return result;
    }
    if (is_listen) {
      result.args.listen = *hp;
      have_listen = true;
    } else {
      result.args.backends.push_back(*hp);
    }
  }

  if (!have_listen) {
    result.error = "--listen is required";
    return result;
  }
  if (result.args.backends.empty()) {
    result.error = "at least one --backend is required";
    return result;
  }
  result.kind = ParseResult::Kind::kRun;
  return result;
}

const char* usage_text() {
  return "usage: fulcrum --listen HOST:PORT --backend HOST:PORT [--backend HOST:PORT ...]\n"
         "       fulcrum --help | --version\n"
         "\n"
         "  --listen HOST:PORT   address to accept clients on (IPv4; port 0 = any free port)\n"
         "  --backend HOST:PORT  backend server; repeat for several\n";
}

}  // namespace fulcrum
