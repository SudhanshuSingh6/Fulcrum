#pragma once

#include <sys/types.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fulcrum::test {

// A child process owned by the test. Move-only; the destructor stops it
// (SIGTERM, then SIGKILL after a grace period) and reaps it.
class Process {
 public:
  // Starts `path` with `args` (not including argv[0]). If `output_file` is
  // set, the child's stdout and stderr go to that file. Aborts the test run on failure.
  static Process spawn(const std::string& path, const std::vector<std::string>& args,
                       const std::string& output_file = "");

  Process() noexcept = default;  // empty: owns no process
  Process(Process&& other) noexcept;
  Process& operator=(Process&& other) noexcept;
  Process(const Process&) = delete;
  Process& operator=(const Process&) = delete;
  ~Process();

  pid_t pid() const noexcept { return pid_; }

  // Waits up to `timeout` for the process to exit by itself. Returns the wait
  // status, or nullopt if it is still running.
  std::optional<int> wait_exit(std::chrono::milliseconds timeout);

  // Sends SIGTERM, waits up to `grace`, then SIGKILL. Returns the wait status.
  int stop(std::chrono::milliseconds grace = std::chrono::milliseconds(2000));

 private:
  explicit Process(pid_t pid) noexcept : pid_(pid) {}
  pid_t pid_ = -1;
};

// Asks the kernel for a currently unused TCP port on 127.0.0.1.
std::uint16_t free_port();

// Returns true once something accepts TCP connections on 127.0.0.1:port.
bool wait_for_port(std::uint16_t port, std::chrono::milliseconds timeout);

}  // namespace fulcrum::test
