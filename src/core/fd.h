#pragma once

namespace fulcrum {

// Owns one file descriptor and closes it when destroyed. Move-only, so there
// is always exactly one owner.
class Fd {
 public:
  Fd() noexcept = default;
  explicit Fd(int fd) noexcept : fd_(fd) {}

  Fd(Fd&& other) noexcept : fd_(other.release()) {}
  Fd& operator=(Fd&& other) noexcept {
    if (this != &other) reset(other.release());
    return *this;
  }
  Fd(const Fd&) = delete;
  Fd& operator=(const Fd&) = delete;

  ~Fd() { reset(); }

  int get() const noexcept { return fd_; }
  bool valid() const noexcept { return fd_ >= 0; }
  explicit operator bool() const noexcept { return valid(); }

  // Gives up ownership without closing; the caller now owns the fd.
  [[nodiscard]] int release() noexcept {
    const int fd = fd_;
    fd_ = -1;
    return fd;
  }

  // Closes the current fd (if any) and takes ownership of `fd`.
  void reset(int fd = -1) noexcept;

 private:
  int fd_ = -1;
};

}  // namespace fulcrum
