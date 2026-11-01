#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "process.h"

namespace fulcrum::test {

// Starts fake backends (ids "a", "b", "c", ...) on free ports and a Fulcrum
// instance pointing at them. Everything is stopped when the fixture is destroyed.
class ProxyFixture {
 public:
  // Starts the backends only. Call start_fulcrum() afterwards.
  explicit ProxyFixture(int num_backends);

  // Starts Fulcrum with --listen 127.0.0.1:<free port> and one --backend per
  // backend. Returns false if its port didn't open within 5 s.
  bool start_fulcrum(const std::vector<std::string>& extra_args = {});

  std::uint16_t fulcrum_port() const { return fulcrum_port_; }
  std::uint16_t backend_port(std::size_t i) const { return backend_ports_.at(i); }
  Process& fulcrum() { return fulcrum_; }

 private:
  std::vector<std::uint16_t> backend_ports_;
  std::vector<Process> backends_;
  std::uint16_t fulcrum_port_ = 0;
  Process fulcrum_;
};

}  // namespace fulcrum::test
