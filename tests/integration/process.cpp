#include "process.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

extern char** environ;

namespace fulcrum::test {

namespace {

sockaddr_in loopback(std::uint16_t port) {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  return addr;
}

}  // namespace

Process Process::spawn(const std::string& path, const std::vector<std::string>& args,
                       const std::string& output_file) {
  std::vector<char*> argv;
  argv.push_back(const_cast<char*>(path.c_str()));
  for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
  argv.push_back(nullptr);

  // File actions run in the child between fork and exec: open the file as
  // stdout, then duplicate it onto stderr.
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  if (!output_file.empty()) {
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, output_file.c_str(),
                                     O_WRONLY | O_CREAT | O_TRUNC, 0644);
    posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
  }

  pid_t pid = -1;
  const int rc = posix_spawn(&pid, path.c_str(), &actions, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  if (rc != 0) {
    std::fprintf(stderr, "posix_spawn(%s) failed: %s\n", path.c_str(), std::strerror(rc));
    std::abort();
  }
  return Process(pid);
}

Process::Process(Process&& other) noexcept : pid_(other.pid_) { other.pid_ = -1; }

Process& Process::operator=(Process&& other) noexcept {
  if (this != &other) {
    stop();
    pid_ = other.pid_;
    other.pid_ = -1;
  }
  return *this;
}

Process::~Process() { stop(); }

std::optional<int> Process::wait_exit(std::chrono::milliseconds timeout) {
  if (pid_ <= 0) return 0;
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  for (;;) {
    int status = 0;
    const pid_t r = ::waitpid(pid_, &status, WNOHANG);
    if (r == pid_ || (r < 0 && errno != EINTR)) {
      pid_ = -1;
      return status;
    }
    if (std::chrono::steady_clock::now() >= deadline) return std::nullopt;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

int Process::stop(std::chrono::milliseconds grace) {
  if (pid_ <= 0) return 0;
  ::kill(pid_, SIGTERM);
  if (auto status = wait_exit(grace)) return *status;
  int status = 0;
  ::kill(pid_, SIGKILL);
  while (::waitpid(pid_, &status, 0) < 0 && errno == EINTR) {
  }
  pid_ = -1;
  return status;
}

std::uint16_t free_port() {
  const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0) std::abort();
  sockaddr_in addr = loopback(0);
  socklen_t len = sizeof(addr);
  if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
      ::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
    std::abort();
  }
  ::close(fd);
  return ntohs(addr.sin_port);
}

bool wait_for_port(std::uint16_t port, std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return false;
    sockaddr_in addr = loopback(port);
    const int rc = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    ::close(fd);
    if (rc == 0) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  return false;
}

}  // namespace fulcrum::test
