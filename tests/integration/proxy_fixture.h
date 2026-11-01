#pragma once

#include <chrono>
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
  // backend. Returns false if it didn't log "listening on" within 5 s.
  //
  // Readiness comes from the log on purpose: probing the port with a test
  // connection would count as a client (taking a round-robin slot and
  // connecting to a backend), which skews tests that count requests.
  bool start_fulcrum(const std::vector<std::string>& extra_args = {});

  // Starts Fulcrum pointing at the given backend ports instead of the fixture's
  // own backends (e.g. blackholes or closed ports).
  bool start_fulcrum_with_backends(const std::vector<std::uint16_t>& ports);

  // Stops backend i (SIGTERM, then SIGKILL) so its port refuses connections.
  void kill_backend(std::size_t i) { backends_.at(i).stop(); }

  std::uint16_t fulcrum_port() const { return fulcrum_port_; }
  std::uint16_t backend_port(std::size_t i) const { return backend_ports_.at(i); }
  Process& fulcrum() { return fulcrum_; }

  // Everything Fulcrum has written to stdout/stderr so far.
  std::string fulcrum_log() const;

  // Waits up to `timeout` for `text` to appear in Fulcrum's log.
  bool wait_for_log(const std::string& text, std::chrono::milliseconds timeout) const;

 private:
  std::vector<std::uint16_t> backend_ports_;
  std::vector<Process> backends_;
  bool launch(const std::vector<std::string>& args);

  std::uint16_t fulcrum_port_ = 0;
  std::string log_path_;
  Process fulcrum_;
};

}  // namespace fulcrum::test
