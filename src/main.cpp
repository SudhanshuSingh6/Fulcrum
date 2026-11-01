#include <spdlog/spdlog.h>

#include <cstdio>
#include <string>
#include <utility>

#include "control/args.h"
#include "core/listener.h"
#include "core/socket_util.h"
#include "core/version.h"
#include "proxy/tcp_proxy.h"

namespace {
constexpr int kListenBacklog = 1024;
}

int main(int argc, char** argv) {
  using fulcrum::ParseResult;
  const ParseResult parsed = fulcrum::parse_args(argc, argv);
  switch (parsed.kind) {
    case ParseResult::Kind::kHelp:
      std::fputs(fulcrum::usage_text(), stdout);
      return 0;
    case ParseResult::Kind::kVersion:
      std::printf("fulcrum %s\n", std::string(fulcrum::version()).c_str());
      return 0;
    case ParseResult::Kind::kError:
      std::fprintf(stderr, "fulcrum: %s\n\n%s", parsed.error.c_str(), fulcrum::usage_text());
      return 2;
    case ParseResult::Kind::kRun:
      break;
  }

  std::string error;
  fulcrum::Fd listener = fulcrum::listen_tcp(parsed.args.listen, kListenBacklog, error);
  if (!listener) {
    spdlog::error("cannot listen on {}: {}", parsed.args.listen.to_string(), error);
    return 1;
  }
  spdlog::info("fulcrum {} listening on {}:{} with {} backend(s)", fulcrum::version(),
               parsed.args.listen.host, fulcrum::local_port(listener), parsed.args.backends.size());

  fulcrum::TcpProxy proxy(std::move(listener), parsed.args.backends);
  proxy.run();
  return 1;  // run() only returns on a fatal error
}
