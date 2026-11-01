#include "proxy_fixture.h"

#include <gtest/gtest.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>

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

namespace {

std::vector<std::string> fulcrum_args(std::uint16_t listen_port,
                                      const std::vector<std::uint16_t>& backend_ports) {
  std::vector<std::string> args{"--listen", "127.0.0.1:" + std::to_string(listen_port)};
  for (const auto port : backend_ports) {
    args.push_back("--backend");
    args.push_back("127.0.0.1:" + std::to_string(port));
  }
  return args;
}

}  // namespace

bool ProxyFixture::launch(const std::vector<std::string>& args) {
  static std::atomic<int> counter{0};
  log_path_ = ::testing::TempDir() + "fulcrum-" + std::to_string(::getpid()) + "-" +
              std::to_string(counter++) + ".log";
  fulcrum_ = Process::spawn(FULCRUM_BIN, args, log_path_);
  return wait_for_log("listening on", 5s);
}

bool ProxyFixture::start_fulcrum(const std::vector<std::string>& extra_args) {
  fulcrum_port_ = free_port();
  auto args = fulcrum_args(fulcrum_port_, backend_ports_);
  args.insert(args.end(), extra_args.begin(), extra_args.end());
  return launch(args);
}

bool ProxyFixture::start_fulcrum_with_backends(const std::vector<std::uint16_t>& ports) {
  fulcrum_port_ = free_port();
  return launch(fulcrum_args(fulcrum_port_, ports));
}

std::string ProxyFixture::fulcrum_log() const {
  std::ifstream in(log_path_);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

bool ProxyFixture::wait_for_log(const std::string& text, std::chrono::milliseconds timeout) const {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (fulcrum_log().find(text) != std::string::npos) return true;
    std::this_thread::sleep_for(20ms);
  }
  return false;
}

}  // namespace fulcrum::test
