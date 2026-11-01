#include "proxy_fixture.h"

#include <chrono>
#include <stdexcept>

namespace fulcrum::test {

using namespace std::chrono_literals;

ProxyFixture::ProxyFixture(int num_backends) {
  for (int i = 0; i < num_backends; ++i) {
    const std::uint16_t port = free_port();
    const std::string id(1, static_cast<char>('a' + i));
    backends_.push_back(
        Process::spawn(FAKE_BACKEND_BIN, {"--port", std::to_string(port), "--id", id}));
    backend_ports_.push_back(port);
  }
  for (const auto port : backend_ports_) {
    if (!wait_for_port(port, 5s)) throw std::runtime_error("fake_backend did not start");
  }
}

bool ProxyFixture::start_fulcrum(const std::vector<std::string>& extra_args) {
  fulcrum_port_ = free_port();
  std::vector<std::string> args{"--listen", "127.0.0.1:" + std::to_string(fulcrum_port_)};
  for (const auto port : backend_ports_) {
    args.push_back("--backend");
    args.push_back("127.0.0.1:" + std::to_string(port));
  }
  args.insert(args.end(), extra_args.begin(), extra_args.end());
  fulcrum_ = Process::spawn(FULCRUM_BIN, args);
  return wait_for_port(fulcrum_port_, 5s);
}

}  // namespace fulcrum::test
