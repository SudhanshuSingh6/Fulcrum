#include "core/fd.h"

#include <unistd.h>

namespace fulcrum {

void Fd::reset(int fd) noexcept {
  if (fd_ >= 0 && fd_ != fd) {
    // Don't retry close() on EINTR: on Linux the fd is already released, and a
    // retry could close an fd another thread just opened (see man 2 close).
    ::close(fd_);
  }
  fd_ = fd;
}

}  // namespace fulcrum
